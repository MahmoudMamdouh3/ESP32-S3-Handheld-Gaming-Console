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

    // SPlit 16-bit register helpers and Z80 I/O port handlers
static void sms_write_word(sms_context_t* ctx, uint16_t addr, uint16_t val) {
    sms_write_byte(ctx, addr, (uint8_t)(val & 0xFF));
    sms_write_byte(ctx, (uint16_t)(addr + 1), (uint8_t)(val >> 8));
}

static uint16_t sms_read_word(sms_context_t* ctx, uint16_t addr) {
    return (uint16_t)sms_read_byte(ctx, addr) | ((uint16_t)sms_read_byte(ctx, (uint16_t)(addr + 1)) << 8);
}

static uint8_t sms_io_read(sms_context_t* ctx, uint8_t port) {
    if (port >= 0x40 && port <= 0x7F) {
        // VCounter (even) / HCounter (odd)
        return (port & 1) ? 0x00 : ctx->vdp.status;
    } else if (port >= 0x80 && port <= 0xBF) {
        if ((port & 1) == 0) {
            // VDP Data Port (0xBE)
            uint8_t ret = ctx->vdp.read_buf;
            ctx->vdp.read_buf = ctx->vdp.vram[ctx->vdp.addr & 0x3FFF];
            ctx->vdp.addr = (ctx->vdp.addr + 1) & 0x3FFF;
            ctx->vdp.latch = 0;
            return ret;
        } else {
            // VDP Status Port (0xBF)
            uint8_t st = ctx->vdp.vblank_irq ? 0x80 : 0x00;
            ctx->vdp.vblank_irq = false;
            ctx->vdp.latch = 0;
            return st;
        }
    } else if (port >= 0xC0) {
        // Joypad Port A (0xDC) / Port B (0xDD)
        return (port & 1) ? ctx->system_pad : ctx->joypad;
    }
    return 0xFF;
}

static void sms_io_write(sms_context_t* ctx, uint8_t port, uint8_t val) {
    if (port >= 0x80 && port <= 0xBF) {
        if ((port & 1) == 0) {
            // VDP Data Port (0xBE)
            uint8_t code = (ctx->vdp.addr >> 14) & 3;
            if (code == 3) {
                if (ctx->vdp.is_gamegear) {
                    uint8_t cidx = ctx->vdp.addr & 0x3F;
                    ctx->vdp.cram[cidx] = val;
                    uint8_t pal_idx = cidx >> 1;
                    uint16_t gg_col = (uint16_t)ctx->vdp.cram[pal_idx * 2] |
                                      ((uint16_t)ctx->vdp.cram[pal_idx * 2 + 1] << 8);
                    uint8_t r = (uint8_t)(((gg_col >> 0) & 0x0F) * 17);
                    uint8_t g = (uint8_t)(((gg_col >> 4) & 0x0F) * 17);
                    uint8_t b = (uint8_t)(((gg_col >> 8) & 0x0F) * 17);
                    ctx->vdp.palette_cache[pal_idx] = sms_rgb_to_bgr565(r, g, b);
                } else {
                    uint8_t cidx = ctx->vdp.addr & 0x1F;
                    ctx->vdp.cram[cidx] = val;
                    uint8_t r = ((val >> 0) & 3) * 85;
                    uint8_t g = ((val >> 2) & 3) * 85;
                    uint8_t b = ((val >> 4) & 3) * 85;
                    ctx->vdp.palette_cache[cidx] = sms_rgb_to_bgr565(r, g, b);
                }
            } else {
                ctx->vdp.vram[ctx->vdp.addr & 0x3FFF] = val;
            }
            ctx->vdp.read_buf = val;
            ctx->vdp.addr = (ctx->vdp.addr & 0xC000) | ((ctx->vdp.addr + 1) & 0x3FFF);
            ctx->vdp.latch = 0;
        } else {
            // VDP Control Port (0xBF)
            if (ctx->vdp.latch == 0) {
                ctx->vdp.addr = (ctx->vdp.addr & 0xFF00) | val;
                ctx->vdp.latch = 1;
            } else {
                ctx->vdp.addr = (ctx->vdp.addr & 0x00FF) | ((uint16_t)val << 8);
                ctx->vdp.latch = 0;
                uint8_t code = (val >> 6) & 3;
                if (code == 0) {
                    ctx->vdp.read_buf = ctx->vdp.vram[ctx->vdp.addr & 0x3FFF];
                    ctx->vdp.addr = (ctx->vdp.addr & 0xC000) | ((ctx->vdp.addr + 1) & 0x3FFF);
                } else if (code == 2) {
                    uint8_t reg = val & 0x0F;
                    ctx->vdp.regs[reg] = (uint8_t)(ctx->vdp.addr & 0xFF);
                }
            }
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
            int out_x = (col * 8) + px;
            if (out_x < 256) {
                dst[out_x] = ctx->vdp.palette_cache[cram_idx & 0x1F];
            }
        }
    }
}

void sms_run_frame(sms_context_t* ctx) {
    if (!ctx) return;
    
    // Simulate 262 scanlines per 60Hz NTSC frame
    for (int line = 0; line < 262; line++) {
        ctx->vdp.status = (uint8_t)(line <= 0xDA ? line : (line - 6));
        if (line < 192) {
            sms_render_scanline(ctx, line);
        } else if (line == 192) {
            ctx->vdp.vblank_irq = true;
            if (ctx->iff1 && (ctx->vdp.regs[1] & 0x20)) {
                ctx->halted = false;
                ctx->iff1 = 0;
                ctx->sp -= 2;
                sms_write_word(ctx, ctx->sp, ctx->pc);
                ctx->pc = 0x0038;
            }
        }
        
        // Step Z80 instruction cycle budget per line (~228 cycles)
        int cycles = 0;
        while (cycles < 228 && !ctx->halted) {
            uint8_t op = sms_read_byte(ctx, ctx->pc++);
            cycles += 4;
            switch (op) {
                case 0x00: break; // NOP
                case 0x01: ctx->bc = sms_read_word(ctx, ctx->pc); ctx->pc += 2; cycles += 6; break;
                case 0x11: ctx->de = sms_read_word(ctx, ctx->pc); ctx->pc += 2; cycles += 6; break;
                case 0x21: ctx->hl = sms_read_word(ctx, ctx->pc); ctx->pc += 2; cycles += 6; break;
                case 0x31: ctx->sp = sms_read_word(ctx, ctx->pc); ctx->pc += 2; cycles += 6; break;
                case 0x06: ctx->bc = (ctx->bc & 0x00FF) | ((uint16_t)sms_read_byte(ctx, ctx->pc++) << 8); cycles += 3; break;
                case 0x0E: ctx->bc = (ctx->bc & 0xFF00) | sms_read_byte(ctx, ctx->pc++); cycles += 3; break;
                case 0x16: ctx->de = (ctx->de & 0x00FF) | ((uint16_t)sms_read_byte(ctx, ctx->pc++) << 8); cycles += 3; break;
                case 0x1E: ctx->de = (ctx->de & 0xFF00) | sms_read_byte(ctx, ctx->pc++); cycles += 3; break;
                case 0x26: ctx->hl = (ctx->hl & 0x00FF) | ((uint16_t)sms_read_byte(ctx, ctx->pc++) << 8); cycles += 3; break;
                case 0x2E: ctx->hl = (ctx->hl & 0xFF00) | sms_read_byte(ctx, ctx->pc++); cycles += 3; break;
                case 0x36: sms_write_byte(ctx, ctx->hl, sms_read_byte(ctx, ctx->pc++)); cycles += 6; break;
                case 0x3E: ctx->af = (ctx->af & 0x00FF) | ((uint16_t)sms_read_byte(ctx, ctx->pc++) << 8); cycles += 3; break;
                case 0x32: { uint16_t a = sms_read_word(ctx, ctx->pc); ctx->pc += 2; sms_write_byte(ctx, a, (uint8_t)(ctx->af >> 8)); cycles += 9; break; }
                case 0x3A: { uint16_t a = sms_read_word(ctx, ctx->pc); ctx->pc += 2; ctx->af = (ctx->af & 0x00FF) | ((uint16_t)sms_read_byte(ctx, a) << 8); cycles += 9; break; }
                case 0x77: sms_write_byte(ctx, ctx->hl, (uint8_t)(ctx->af >> 8)); cycles += 3; break;
                case 0x7E: ctx->af = (ctx->af & 0x00FF) | ((uint16_t)sms_read_byte(ctx, ctx->hl) << 8); cycles += 3; break;
                case 0x03: ctx->bc++; cycles += 2; break;
                case 0x13: ctx->de++; cycles += 2; break;
                case 0x23: ctx->hl++; cycles += 2; break;
                case 0x0B: ctx->bc--; cycles += 2; break;
                case 0x1B: ctx->de--; cycles += 2; break;
                case 0x2B: ctx->hl--; cycles += 2; break;
                case 0x04: { uint8_t b = (uint8_t)(ctx->bc >> 8) + 1; ctx->bc = (ctx->bc & 0x00FF) | ((uint16_t)b << 8); ctx->af = (ctx->af & 0xFF00) | (b == 0 ? 0x40 : 0); break; }
                case 0x05: { uint8_t b = (uint8_t)(ctx->bc >> 8) - 1; ctx->bc = (ctx->bc & 0x00FF) | ((uint16_t)b << 8); ctx->af = (ctx->af & 0xFF00) | (b == 0 ? 0x40 : 0); break; }
                case 0x0C: { uint8_t c = (uint8_t)(ctx->bc & 0xFF) + 1; ctx->bc = (ctx->bc & 0xFF00) | c; ctx->af = (ctx->af & 0xFF00) | (c == 0 ? 0x40 : 0); break; }
                case 0x0D: { uint8_t c = (uint8_t)(ctx->bc & 0xFF) - 1; ctx->bc = (ctx->bc & 0xFF00) | c; ctx->af = (ctx->af & 0xFF00) | (c == 0 ? 0x40 : 0); break; }
                case 0x3C: { uint8_t a = (uint8_t)(ctx->af >> 8) + 1; ctx->af = ((uint16_t)a << 8) | (a == 0 ? 0x40 : 0); break; }
                case 0x3D: { uint8_t a = (uint8_t)(ctx->af >> 8) - 1; ctx->af = ((uint16_t)a << 8) | (a == 0 ? 0x40 : 0); break; }
                case 0xAF: ctx->af = 0x0040; break; // XOR A
                case 0xB7: { uint8_t a = (uint8_t)(ctx->af >> 8); ctx->af = ((uint16_t)a << 8) | (a == 0 ? 0x40 : 0); break; } // OR A
                case 0xFE: { // CP n
                    uint8_t n = sms_read_byte(ctx, ctx->pc++);
                    uint8_t a = (uint8_t)(ctx->af >> 8);
                    ctx->af = (ctx->af & 0xFF00) | (a == n ? 0x40 : 0) | (a < n ? 0x01 : 0);
                    cycles += 3;
                    break;
                }
                case 0x10: { // DJNZ e
                    int8_t disp = (int8_t)sms_read_byte(ctx, ctx->pc++);
                    uint8_t b = (uint8_t)(ctx->bc >> 8) - 1;
                    ctx->bc = (ctx->bc & 0x00FF) | ((uint16_t)b << 8);
                    if (b != 0) { ctx->pc = (uint16_t)(ctx->pc + disp); cycles += 9; } else { cycles += 4; }
                    break;
                }
                case 0x18: { int8_t disp = (int8_t)sms_read_byte(ctx, ctx->pc++); ctx->pc = (uint16_t)(ctx->pc + disp); cycles += 8; break; } // JR e
                case 0x20: { int8_t disp = (int8_t)sms_read_byte(ctx, ctx->pc++); if (!(ctx->af & 0x40)) { ctx->pc = (uint16_t)(ctx->pc + disp); cycles += 8; } else cycles += 3; break; } // JR NZ
                case 0x28: { int8_t disp = (int8_t)sms_read_byte(ctx, ctx->pc++); if (ctx->af & 0x40) { ctx->pc = (uint16_t)(ctx->pc + disp); cycles += 8; } else cycles += 3; break; } // JR Z
                case 0xC3: ctx->pc = sms_read_word(ctx, ctx->pc); cycles += 6; break; // JP nn
                case 0xC2: { uint16_t dst = sms_read_word(ctx, ctx->pc); ctx->pc += 2; if (!(ctx->af & 0x40)) ctx->pc = dst; cycles += 6; break; } // JP NZ, nn
                case 0xCA: { uint16_t dst = sms_read_word(ctx, ctx->pc); ctx->pc += 2; if (ctx->af & 0x40) ctx->pc = dst; cycles += 6; break; } // JP Z, nn
                case 0xCD: { uint16_t dst = sms_read_word(ctx, ctx->pc); ctx->pc += 2; ctx->sp -= 2; sms_write_word(ctx, ctx->sp, ctx->pc); ctx->pc = dst; cycles += 13; break; } // CALL nn
                case 0xC9: ctx->pc = sms_read_word(ctx, ctx->sp); ctx->sp += 2; cycles += 6; break; // RET
                case 0xC5: ctx->sp -= 2; sms_write_word(ctx, ctx->sp, ctx->bc); cycles += 7; break; // PUSH BC
                case 0xD5: ctx->sp -= 2; sms_write_word(ctx, ctx->sp, ctx->de); cycles += 7; break; // PUSH DE
                case 0xE5: ctx->sp -= 2; sms_write_word(ctx, ctx->sp, ctx->hl); cycles += 7; break; // PUSH HL
                case 0xF5: ctx->sp -= 2; sms_write_word(ctx, ctx->sp, ctx->af); cycles += 7; break; // PUSH AF
                case 0xC1: ctx->bc = sms_read_word(ctx, ctx->sp); ctx->sp += 2; cycles += 6; break; // POP BC
                case 0xD1: ctx->de = sms_read_word(ctx, ctx->sp); ctx->sp += 2; cycles += 6; break; // POP DE
                case 0xE1: ctx->hl = sms_read_word(ctx, ctx->sp); ctx->sp += 2; cycles += 6; break; // POP HL
                case 0xF1: ctx->af = sms_read_word(ctx, ctx->sp); ctx->sp += 2; cycles += 6; break; // POP AF
                case 0xD3: { uint8_t port = sms_read_byte(ctx, ctx->pc++); sms_io_write(ctx, port, (uint8_t)(ctx->af >> 8)); cycles += 7; break; } // OUT (n), A
                case 0xDB: { uint8_t port = sms_read_byte(ctx, ctx->pc++); ctx->af = (ctx->af & 0x00FF) | ((uint16_t)sms_io_read(ctx, port) << 8); cycles += 7; break; } // IN A, (n)
                case 0xF3: ctx->iff1 = ctx->iff2 = 0; break; // DI
                case 0xFB: ctx->iff1 = ctx->iff2 = 1; break; // EI
                case 0x76: ctx->halted = true; break; // HALT
                case 0xED: {
                    uint8_t sub = sms_read_byte(ctx, ctx->pc++);
                    if (sub == 0xB0) { // LDIR
                        while (ctx->bc != 0) {
                            sms_write_byte(ctx, ctx->de++, sms_read_byte(ctx, ctx->hl++));
                            ctx->bc--;
                            cycles += 21;
                        }
                    } else if (sub == 0xB3) { // OTIR
                        uint8_t b = (uint8_t)(ctx->bc >> 8);
                        uint8_t c = (uint8_t)(ctx->bc & 0xFF);
                        while (b != 0) {
                            sms_io_write(ctx, c, sms_read_byte(ctx, ctx->hl++));
                            b--;
                            cycles += 21;
                        }
                        ctx->bc = c;
                    } else if (sub == 0x56) { // IM 1
                        ctx->im = 1;
                    } else if (sub == 0x4D || sub == 0x45) { // RETI / RETN
                        ctx->pc = sms_read_word(ctx, ctx->sp);
                        ctx->sp += 2;
                        ctx->iff1 = ctx->iff2;
                        cycles += 10;
                    }
                    break;
                }
                default: break;
            }
        }
    }
}
