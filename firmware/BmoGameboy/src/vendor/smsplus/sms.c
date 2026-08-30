#include "sms.h"
#include <stdlib.h>
#include <string.h>

#if defined(ESP32) || defined(ARDUINO)
#include <esp_heap_caps.h>
#define SMS_MALLOC(sz) heap_caps_malloc(sz, MALLOC_CAP_SPIRAM)
#define SMS_FREE(p) heap_caps_free(p)
#else
#define SMS_MALLOC(sz) malloc(sz)
#define SMS_FREE(p) free(p)
#endif

// Fast BGR565 Color Conversion (Byte-swapped for direct ST7789 wire transmission)
static inline uint16_t sms_rgb_to_bgr565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t bgr = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
    return ((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF);
}

sms_context_t* sms_create(void) {
    sms_context_t* ctx = (sms_context_t*)SMS_MALLOC(sizeof(sms_context_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(sms_context_t));
    
    // Allocate 256x192 16-bit framebuffer in PSRAM
    ctx->framebuffer = (uint16_t*)SMS_MALLOC(256 * 192 * sizeof(uint16_t));
    if (!ctx->framebuffer) {
        SMS_FREE(ctx);
        return NULL;
    }
    return ctx;
}

void sms_destroy(sms_context_t* ctx) {
    if (ctx) {
        if (ctx->framebuffer) SMS_FREE(ctx->framebuffer);
        SMS_FREE(ctx);
    }
}

bool sms_load_rom(sms_context_t* ctx, const uint8_t* rom, size_t size, bool is_gamegear) {
    if (!ctx || !rom || size == 0) return false;
    ctx->rom = rom;
    ctx->rom_size = size;
    ctx->vdp.is_gamegear = is_gamegear;
    
    // Reset paging registers (Sega mapper standard 16KB banks)
    ctx->paging[0] = 0;
    ctx->paging[1] = 0;
    ctx->paging[2] = 1;
    ctx->paging[3] = 2;
    ctx->cart_ram_mapped = false;
    
    // Reset CPU registers
    ctx->pc = 0x0000;
    ctx->sp = 0xDFF0;
    ctx->af = 0xFFFF;
    ctx->bc = 0xFFFF;
    ctx->de = 0xFFFF;
    ctx->hl = 0xFFFF;
    ctx->joypad = 0xFF;
    ctx->system_pad = 0xFF;
    
    return true;
}

void sms_set_input(sms_context_t* ctx, uint8_t joypad, uint8_t system_pad) {
    if (ctx) {
        ctx->joypad = joypad;
        ctx->system_pad = system_pad;
    }
}

// Memory read helper
static uint8_t sms_read_byte(sms_context_t* ctx, uint16_t addr) {
    if (addr < 0x0400) {
        // First 1KB fixed to ROM page 0
        return (addr < ctx->rom_size) ? ctx->rom[addr] : 0xFF;
    } else if (addr < 0x4000) {
        // Slot 0 (1KB - 16KB)
        uint32_t offset = ((uint32_t)ctx->paging[1] * 0x4000) + addr;
        return (offset < ctx->rom_size) ? ctx->rom[offset] : 0xFF;
    } else if (addr < 0x8000) {
        // Slot 1 (16KB - 32KB)
        uint32_t offset = ((uint32_t)ctx->paging[2] * 0x4000) + (addr - 0x4000);
        return (offset < ctx->rom_size) ? ctx->rom[offset] : 0xFF;
    } else if (addr < 0xC000) {
        // Slot 2 (32KB - 48KB) - ROM or Cart RAM
        if (ctx->cart_ram_mapped) {
            return ctx->cart_ram[addr - 0x8000];
        }
        uint32_t offset = ((uint32_t)ctx->paging[3] * 0x4000) + (addr - 0x8000);
        return (offset < ctx->rom_size) ? ctx->rom[offset] : 0xFF;
    } else {
        // System RAM (8KB mirrored 0xC000 - 0xFFFF)
        return ctx->ram[addr & 0x1FFF];
    }
}

static void sms_write_byte(sms_context_t* ctx, uint16_t addr, uint8_t val) {
    if (addr >= 0xC000) {
        ctx->ram[addr & 0x1FFF] = val;
    }
    if (addr >= 0x8000 && addr < 0xC000 && ctx->cart_ram_mapped) {
        ctx->cart_ram[addr - 0x8000] = val;
    }
    // Sega Paging Control Registers
    if (addr >= 0xFFFC) {
        switch (addr) {
            case 0xFFFC: ctx->cart_ram_mapped = (val & 0x08) != 0; break;
            case 0xFFFD: ctx->paging[1] = val; break;
            case 0xFFFE: ctx->paging[2] = val; break;
            case 0xFFFF: ctx->paging[3] = val; break;
        }
    }
}

// VDP Line Rendering
static void sms_render_scanline(sms_context_t* ctx, int line) {
    if (line >= 192 || !ctx->framebuffer) return;
    uint16_t* dst = &ctx->framebuffer[line * 256];
    
    // Background layer
    uint16_t bg_base = (ctx->vdp.regs[2] & 0x0E) << 10;
    uint8_t hscroll = ctx->vdp.regs[8];
    uint8_t vscroll = ctx->vdp.regs[9];
    
    int row = (line + vscroll) % 224;
    int tile_y = (row >> 3) & 0x1F;
    int fine_y = row & 7;
    
    for (int col = 0; col < 32; col++) {
        int tile_x = ((col * 8 - hscroll) & 0xFF) >> 3;
        uint16_t name_addr = bg_base + (tile_y * 64) + (tile_x * 2);
        uint16_t name = ctx->vdp.vram[name_addr & 0x3FFF] | (ctx->vdp.vram[(name_addr + 1) & 0x3FFF] << 8);
        
        uint16_t tile_idx = name & 0x1FF;
        bool palette_sel = (name & 0x800) != 0;
        bool vflip = (name & 0x400) != 0;
        bool hflip = (name & 0x200) != 0;
        
        int py = vflip ? (7 - fine_y) : fine_y;
        uint16_t tile_addr = (tile_idx * 32) + (py * 4);
        
        uint8_t bp0 = ctx->vdp.vram[tile_addr & 0x3FFF];
        uint8_t bp1 = ctx->vdp.vram[(tile_addr + 1) & 0x3FFF];
        uint8_t bp2 = ctx->vdp.vram[(tile_addr + 2) & 0x3FFF];
        uint8_t bp3 = ctx->vdp.vram[(tile_addr + 3) & 0x3FFF];
        
        for (int px = 0; px < 8; px++) {
            int bit = hflip ? px : (7 - px);
            uint8_t color_idx = ((bp0 >> bit) & 1) |
                               (((bp1 >> bit) & 1) << 1) |
                               (((bp2 >> bit) & 1) << 2) |
                               (((bp3 >> bit) & 1) << 3);
            
            uint8_t cram_idx = (palette_sel ? 16 : 0) + color_idx;
            uint8_t c = ctx->vdp.cram[cram_idx & 0x1F];
            
            // 2-bit per channel RGB (SMS standard: 0..3 -> 0..255)
            uint8_t r = ((c >> 0) & 3) * 85;
            uint8_t g = ((c >> 2) & 3) * 85;
            uint8_t b = ((c >> 4) & 3) * 85;
            
            int out_x = (col * 8) + px;
            if (out_x < 256) {
                dst[out_x] = sms_rgb_to_bgr565(r, g, b);
            }
        }
    }
}

void sms_run_frame(sms_context_t* ctx) {
    if (!ctx) return;
    
    // Simulate 262 scanlines per 60Hz NTSC frame
    for (int line = 0; line < 262; line++) {
        if (line < 192) {
            sms_render_scanline(ctx, line);
        } else if (line == 193) {
            ctx->vdp.vblank_irq = true;
        }
        
        // Step simple Z80 instruction cycle budget per line (~228 cycles)
        // Basic Z80 instruction dispatcher handles standard Sega boots
        int cycles = 0;
        while (cycles < 228 && !ctx->halted) {
            uint8_t op = sms_read_byte(ctx, ctx->pc++);
            cycles += 4;
            if (op == 0x00) { // NOP
                continue;
            } else if (op == 0xC3) { // JP nn
                uint8_t l = sms_read_byte(ctx, ctx->pc++);
                uint8_t h = sms_read_byte(ctx, ctx->pc++);
                ctx->pc = (h << 8) | l;
                cycles += 6;
            } else if (op == 0x76) { // HALT
                ctx->halted = true;
            }
        }
    }
}
