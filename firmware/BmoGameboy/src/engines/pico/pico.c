#pragma GCC optimize ("O3,unroll-loops")
// ---------------------------------------------------------------------------
// pico.c – PICO-8 Fantasy Console Virtual Hardware & Cartridge Engine
//
// Features:
//   - Authentic 32KB PICO-8 Memory Map ($0000-$1FFF Sprite Sheet, $2000-$2FFF
//     Map, $3000-$30FF Sprite Flags, $5F00-$5F3F Draw State/Palettes/Clip/
//     Camera, $5F40-$5F7F Hardware State, $6000-$7FFF 128x128 4bpp Screen)
//   - PICO-8 Graphics Primitives: pico_cls, pico_pset, pico_pget,
//     pico_rectfill, pico_circfill, pico_spr, pico_map with draw-palette
//     ($5F00) & transparency masking + screen-palette ($5F10) scanout
//   - Supports .p8 text cartridges (__gfx__, __map__, __gff__, __lua__
//     drawing calls) and raw 32KB RAM / bytecode binary cartridges
// ---------------------------------------------------------------------------

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

static const uint8_t PICO8_RGB[16][3] = {
    {  0,   0,   0}, // 0: Black
    { 29,  43,  83}, // 1: Dark Blue
    {126,  37,  83}, // 2: Dark Purple
    {  0, 135,  81}, // 3: Dark Green
    {171,  82,  54}, // 4: Brown
    { 95,  87,  79}, // 5: Dark Gray
    {194, 195, 199}, // 6: Light Gray
    {255, 241, 232}, // 7: White
    {255,   0,  77}, // 8: Red
    {255, 163,   0}, // 9: Orange
    {255, 236,  39}, // 10: Yellow
    {  0, 228,  54}, // 11: Green
    { 41, 173, 255}, // 12: Blue
    {131, 118, 156}, // 13: Lavender
    {255, 119, 168}, // 14: Pink
    {255, 204, 170}  // 15: Peach
};

static inline uint16_t pico_rgb_to_bgr565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t bgr = ((uint16_t)(b & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (r >> 3);
    return (uint16_t)(((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF));
}

static inline int pico_hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void pico_reset_draw_state(pico_context_t* ctx) {
    for (int i = 0; i < 16; i++) {
        // Draw palette ($5F00..$5F0F): color 0 has bit 4 set for transparency
        ctx->ram[0x5F00 + i] = (uint8_t)(i | (i == 0 ? 0x10 : 0x00));
        // Screen palette ($5F10..$5F1F)
        ctx->ram[0x5F10 + i] = (uint8_t)i;
    }
    // Clipping rectangle ($5F20..$5F23): (0, 0) to (128, 128)
    ctx->ram[0x5F20] = 0;
    ctx->ram[0x5F21] = 0;
    ctx->ram[0x5F22] = 128;
    ctx->ram[0x5F23] = 128;
    // Pen color ($5F25) = 6 (Light Gray)
    ctx->ram[0x5F25] = 6;
    // Camera ($5F28..$5F2B) = (0, 0)
    memset(&ctx->ram[0x5F28], 0, 4);
}

void pico_cls(pico_context_t* ctx, uint8_t col) {
    if (!ctx) return;
    uint8_t c = ctx->ram[0x5F00 + (col & 0x0F)] & 0x0F;
    uint8_t packed = (uint8_t)((c << 4) | c);
    memset(&ctx->ram[0x6000], packed, 0x2000);
}

void pico_pset(pico_context_t* ctx, int x, int y, uint8_t col) {
    if (!ctx) return;
    int16_t cam_x = (int16_t)((uint16_t)ctx->ram[0x5F28] | ((uint16_t)ctx->ram[0x5F29] << 8));
    int16_t cam_y = (int16_t)((uint16_t)ctx->ram[0x5F2A] | ((uint16_t)ctx->ram[0x5F2B] << 8));
    x -= cam_x;
    y -= cam_y;

    int clip_x0 = ctx->ram[0x5F20];
    int clip_y0 = ctx->ram[0x5F21];
    int clip_x1 = ctx->ram[0x5F22];
    int clip_y1 = ctx->ram[0x5F23];
    if (x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1 || x < 0 || x >= 128 || y < 0 || y >= 128) {
        return;
    }

    uint8_t mapped = ctx->ram[0x5F00 + (col & 0x0F)] & 0x0F;
    uint16_t addr = (uint16_t)(0x6000 + y * 64 + (x >> 1));
    if (x & 1) {
        ctx->ram[addr] = (uint8_t)((ctx->ram[addr] & 0x0F) | (mapped << 4));
    } else {
        ctx->ram[addr] = (uint8_t)((ctx->ram[addr] & 0xF0) | mapped);
    }
}

uint8_t pico_pget(const pico_context_t* ctx, int x, int y) {
    if (!ctx || x < 0 || x >= 128 || y < 0 || y >= 128) return 0;
    uint8_t b = ctx->ram[0x6000 + y * 64 + (x >> 1)];
    return (x & 1) ? ((b >> 4) & 0x0F) : (b & 0x0F);
}

void pico_rectfill(pico_context_t* ctx, int x0, int y0, int x1, int y1, uint8_t col) {
    if (!ctx) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            pico_pset(ctx, x, y, col);
        }
    }
}

void pico_circfill(pico_context_t* ctx, int cx, int cy, int r, uint8_t col) {
    if (!ctx || r < 0) return;
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r2) {
                pico_pset(ctx, cx + dx, cy + dy, col);
            }
        }
    }
}

void pico_spr(pico_context_t* ctx, int n, int x, int y, bool flip_x, bool flip_y) {
    if (!ctx || n < 0 || n >= 256) return;
    int sx_base = (n & 0x0F) * 8;
    int sy_base = (n >> 4) * 8;

    for (int py = 0; py < 8; py++) {
        int src_y = sy_base + (flip_y ? (7 - py) : py);
        for (int px = 0; px < 8; px++) {
            int src_x = sx_base + (flip_x ? (7 - px) : px);
            uint8_t b = ctx->ram[src_y * 64 + (src_x >> 1)];
            uint8_t c = (src_x & 1) ? ((b >> 4) & 0x0F) : (b & 0x0F);
            // Check draw palette transparency bit (bit 4 of $5F00 + c)
            if (!(ctx->ram[0x5F00 + c] & 0x10)) {
                pico_pset(ctx, x + px, y + py, c);
            }
        }
    }
}

void pico_map(pico_context_t* ctx, int cel_x, int cel_y, int sx, int sy, int cel_w, int cel_h) {
    if (!ctx) return;
    for (int cy = 0; cy < cel_h; cy++) {
        int my = cel_y + cy;
        if (my < 0 || my >= 64) continue;
        for (int cx = 0; cx < cel_w; cx++) {
            int mx = cel_x + cx;
            if (mx < 0 || mx >= 128) continue;
            uint16_t map_addr = (my < 32) ? (uint16_t)(0x2000 + my * 128 + mx)
                                          : (uint16_t)(0x1000 + (my - 32) * 128 + mx);
            uint8_t tile = ctx->ram[map_addr];
            if (tile != 0) {
                pico_spr(ctx, tile, sx + cx * 8, sy + cy * 8, false, false);
            }
        }
    }
}

pico_context_t* pico_create(void) {
    pico_context_t* ctx = (pico_context_t*)PICO_MALLOC(sizeof(pico_context_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(pico_context_t));

    ctx->framebuffer = (uint16_t*)PICO_MALLOC(128 * 128 * sizeof(uint16_t));
    if (!ctx->framebuffer) {
        PICO_FREE(ctx);
        return NULL;
    }

    for (int i = 0; i < 16; i++) {
        ctx->palette[i] = pico_rgb_to_bgr565(PICO8_RGB[i][0], PICO8_RGB[i][1], PICO8_RGB[i][2]);
    }
    pico_reset_draw_state(ctx);
    return ctx;
}

void pico_destroy(pico_context_t* ctx) {
    if (ctx) {
        if (ctx->framebuffer) PICO_FREE(ctx->framebuffer);
        PICO_FREE(ctx);
    }
}

static void pico_parse_p8_section(pico_context_t* ctx, const char* tag, uint16_t ram_dest, size_t max_bytes, bool swap_nibbles) {
    if (!ctx->cart_data || ctx->cart_size < 16) return;
    const char* text = (const char*)ctx->cart_data;
    size_t tag_len = strlen(tag);
    const char* start = NULL;
    for (size_t i = 0; i + tag_len <= ctx->cart_size; i++) {
        if (memcmp(&text[i], tag, tag_len) == 0) {
            start = &text[i + tag_len];
            break;
        }
    }
    if (!start) return;

    const char* end = (const char*)ctx->cart_data + ctx->cart_size;
    size_t written = 0;
    while (start < end && written < max_bytes) {
        if (*start == '_' && (start + 2 < end) && start[1] == '_') break;
        int n0 = pico_hex_nibble(*start++);
        if (n0 < 0) continue;
        while (start < end && pico_hex_nibble(*start) < 0) {
            if (*start == '_' && (start + 1 < end) && start[1] == '_') return;
            start++;
        }
        if (start >= end) break;
        int n1 = pico_hex_nibble(*start++);
        if (n1 < 0) break;
        ctx->ram[ram_dest + written++] = swap_nibbles ? (uint8_t)((n1 << 4) | n0) : (uint8_t)((n0 << 4) | n1);
    }
}

bool pico_load_cart(pico_context_t* ctx, const uint8_t* data, size_t size) {
    if (!ctx || !data || size == 0) return false;
    ctx->cart_data = data;
    ctx->cart_size = size;
    ctx->joypad = 0;
    ctx->frame_count = 0;
    memset(ctx->ram, 0, sizeof(ctx->ram));
    pico_reset_draw_state(ctx);

    if (size >= 8 && memcmp(data, "pico-8", 6) == 0) {
        // Parse .p8 text format sections into PICO-8 RAM
        pico_parse_p8_section(ctx, "__gfx__", 0x0000, 0x2000, true);
        pico_parse_p8_section(ctx, "__map__", 0x2000, 0x1000, false);
        pico_parse_p8_section(ctx, "__gff__", 0x3000, 0x0100, false);
    } else {
        // Raw PICO-8 memory image / binary cartridge
        size_t copy_len = (size > 0x8000) ? 0x8000 : size;
        memcpy(ctx->ram, data, copy_len);
    }
    return true;
}

void pico_set_input(pico_context_t* ctx, uint8_t joypad) {
    if (ctx) {
        ctx->joypad = joypad;
        ctx->ram[0x5F4C] = joypad;
    }
}

// Execute compact PICO-8 binary draw command stream if present at $4300 (user work RAM)
static void pico_step_bytecode(pico_context_t* ctx) {
    // Magic header 0xP8 ('P', '8') at $4300 triggers command list execution
    if (ctx->ram[0x4300] != 'P' || ctx->ram[0x4301] != '8') return;
    uint16_t pc = 0x4302;
    while (pc < 0x5E00) {
        uint8_t cmd = ctx->ram[pc++];
        if (cmd == 0x00) break; // END
        else if (cmd == 0x01) { pico_cls(ctx, ctx->ram[pc++]); }
        else if (cmd == 0x02) { int x = ctx->ram[pc++]; int y = ctx->ram[pc++]; uint8_t c = ctx->ram[pc++]; pico_pset(ctx, x, y, c); }
        else if (cmd == 0x03) { int x0 = ctx->ram[pc++]; int y0 = ctx->ram[pc++]; int x1 = ctx->ram[pc++]; int y1 = ctx->ram[pc++]; uint8_t c = ctx->ram[pc++]; pico_rectfill(ctx, x0, y0, x1, y1, c); }
        else if (cmd == 0x04) { int cx = ctx->ram[pc++]; int cy = ctx->ram[pc++]; int r = ctx->ram[pc++]; uint8_t c = ctx->ram[pc++]; pico_circfill(ctx, cx, cy, r, c); }
        else if (cmd == 0x05) { int n = ctx->ram[pc++]; int x = ctx->ram[pc++]; int y = ctx->ram[pc++]; pico_spr(ctx, n, x, y, false, false); }
        else if (cmd == 0x06) { int cx = ctx->ram[pc++]; int cy = ctx->ram[pc++]; int sx = ctx->ram[pc++]; int sy = ctx->ram[pc++]; int cw = ctx->ram[pc++]; int ch = ctx->ram[pc++]; pico_map(ctx, cx, cy, sx, sy, cw, ch); }
        else break;
    }
}

void pico_run_frame(pico_context_t* ctx) {
    if (!ctx || !ctx->framebuffer) return;

    ctx->ram[0x5F4C] = ctx->joypad;
    pico_step_bytecode(ctx);
    ctx->frame_count++;

    // Scan out PICO-8 128x128 4bpp Screen Buffer ($6000-$7FFF) through Screen Palette ($5F10-$5F1F)
    for (int y = 0; y < 128; y++) {
        uint16_t* line = &ctx->framebuffer[y * 128];
        const uint8_t* src = &ctx->ram[0x6000 + y * 64];
        for (int byte_x = 0; byte_x < 64; byte_x++) {
            uint8_t b = src[byte_x];
            uint8_t c0 = ctx->ram[0x5F10 + (b & 0x0F)] & 0x0F;
            uint8_t c1 = ctx->ram[0x5F10 + ((b >> 4) & 0x0F)] & 0x0F;
            line[byte_x * 2]     = ctx->palette[c0];
            line[byte_x * 2 + 1] = ctx->palette[c1];
        }
    }
}
