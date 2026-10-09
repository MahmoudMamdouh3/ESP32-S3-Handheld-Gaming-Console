#pragma GCC optimize ("O3,unroll-loops")
// ---------------------------------------------------------------------------
// pce.c – PC Engine / TurboGrafx-16 Core Engine for ESP32-S3
//
// Features:
//   - Hudson Soft HuC6280 8-bit CPU (65C02 + HuC extensions, 7.16 MHz)
//   - HuC6270 Video Display Controller (VDC) with background tile & sprite engine
//   - HuC6260 Video Color Encoder (VCE) with 512-color BGR565 palette
//   - 8KB Work RAM + 2KB Battery Backup RAM + 64KB VDC VRAM
//   - 256x240 frame output in Octal PSRAM for ST7789 display
// ---------------------------------------------------------------------------

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
static inline uint16_t pce_rgb_to_bgr565(uint8_t r3, uint8_t g3, uint8_t b3) {
    uint8_t r8 = (uint8_t)((r3 * 255) / 7);
    uint8_t g8 = (uint8_t)((g3 * 255) / 7);
    uint8_t b8 = (uint8_t)((b3 * 255) / 7);
    uint16_t bgr = ((uint16_t)(b8 & 0xF8) << 8) | ((uint16_t)(g8 & 0xFC) << 3) | (r8 >> 3);
    return (uint16_t)(((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF));
}

// ---------------------------------------------------------------------------
// Memory Read / Write Handlers
// ---------------------------------------------------------------------------
static uint8_t pce_read(pce_context_t* ctx, uint16_t addr) {
    uint8_t page = (addr >> 13) & 7;
    uint8_t bank = ctx->mpr[page];
    uint16_t offset = addr & 0x1FFF;

    if (bank < 0x80) {
        // ROM mapping
        if (ctx->rom && ctx->rom_size > 0) {
            uint32_t rom_addr = ((uint32_t)bank * 0x2000 + offset) % ctx->rom_size;
            return ctx->rom[rom_addr];
        }
        return 0xFF;
    }

    if (bank == 0xF7) {
        // 2KB Battery Backup RAM
        return ctx->bram[offset & 0x7FF];
    }

    if (bank >= 0xF8 && bank <= 0xFB) {
        // 8KB Main Work RAM (mirrored)
        return ctx->ram[offset];
    }

    if (bank == 0xFF) {
        // Hardware I/O space
        if (offset < 0x0400) {
            // VDC (HuC6270)
            uint8_t reg_select = offset & 3;
            if (reg_select == 0) {
                // VDC Status Register
                uint8_t st = (uint8_t)(ctx->vdc_status & 0xFF);
                // Reading status acknowledges raster and vblank flags
                ctx->vdc_status &= ~(0x20 | 0x04);
                return st;
            }
            if (reg_select == 2) {
                // VDC Data LSB
                uint32_t vram_addr = (ctx->vdc_marr * 2) & 0xFFFF;
                return ctx->vram[vram_addr];
            }
            if (reg_select == 3) {
                // VDC Data MSB with auto-increment
                uint32_t vram_addr = (ctx->vdc_marr * 2 + 1) & 0xFFFF;
                uint8_t val = ctx->vram[vram_addr];
                // Increment MARR
                uint8_t inc_step = (ctx->vdc_cr >> 11) & 3;
                static const uint16_t STEPS[4] = { 1, 32, 64, 128 };
                ctx->vdc_marr = (ctx->vdc_marr + STEPS[inc_step]) & 0x7FFF;
                return val;
            }
            return 0;
        }

        if (offset >= 0x0400 && offset < 0x0800) {
            // VCE (HuC6260)
            uint8_t vce_sel = offset & 7;
            if (vce_sel == 4) {
                return (uint8_t)(ctx->vce_raw[ctx->vce_addr & 511] & 0xFF);
            }
            if (vce_sel == 5) {
                uint8_t val = (uint8_t)((ctx->vce_raw[ctx->vce_addr & 511] >> 8) & 0x01);
                ctx->vce_addr = (ctx->vce_addr + 1) & 511;
                return val;
            }
            return 0;
        }

        if (offset >= 0x1000 && offset < 0x1400) {
            // Joypad Port
            if (ctx->joypad_select) {
                // D-Pad nibble: Up, Right, Down, Left
                return (ctx->joypad & 0x0F);
            } else {
                // Buttons nibble: I, II, Select, Run
                return ((ctx->joypad >> 4) & 0x0F);
            }
        }

        if (offset >= 0x1400 && offset < 0x1800) {
            // Interrupt Controller
            if ((offset & 3) == 2) return ctx->irq_mask;
            if ((offset & 3) == 3) return ctx->irq_status;
            return 0;
        }
    }

    return 0xFF;
}

static void pce_write(pce_context_t* ctx, uint16_t addr, uint8_t val) {
    uint8_t page = (addr >> 13) & 7;
    uint8_t bank = ctx->mpr[page];
    uint16_t offset = addr & 0x1FFF;

    if (bank == 0xF7) {
        // Battery Backup RAM
        ctx->bram[offset & 0x7FF] = val;
        return;
    }

    if (bank >= 0xF8 && bank <= 0xFB) {
        // Main Work RAM
        ctx->ram[offset] = val;
        return;
    }

    if (bank == 0xFF) {
        // Hardware I/O space
        if (offset < 0x0400) {
            // VDC Registers
            uint8_t reg_select = offset & 3;
            if (reg_select == 0) {
                ctx->vdc_reg = val & 0x1F;
            } else if (reg_select == 2) {
                ctx->vdc_latch = val;
            } else if (reg_select == 3) {
                uint16_t data = ((uint16_t)val << 8) | ctx->vdc_latch;
                switch (ctx->vdc_reg) {
                    case 0x00: ctx->vdc_mawr = data & 0x7FFF; break;
                    case 0x01: ctx->vdc_marr = data & 0x7FFF; break;
                    case 0x02: {
                        // VRAM Write Data
                        uint32_t vaddr = (ctx->vdc_mawr * 2) & 0xFFFF;
                        ctx->vram[vaddr] = ctx->vdc_latch;
                        ctx->vram[vaddr + 1] = val;
                        uint8_t inc_step = (ctx->vdc_cr >> 11) & 3;
                        static const uint16_t STEPS[4] = { 1, 32, 64, 128 };
                        ctx->vdc_mawr = (ctx->vdc_mawr + STEPS[inc_step]) & 0x7FFF;
                        break;
                    }
                    case 0x05: ctx->vdc_cr = data; break;
                    case 0x06: ctx->vdc_rcr = data & 0x03FF; break;
                    case 0x07: ctx->vdc_bxr = data & 0x03FF; break;
                    case 0x08: ctx->vdc_byr = data & 0x01FF; break;
                    case 0x09: ctx->vdc_mwr = data; break;
                    case 0x13: ctx->vdc_satb = data & 0x7FFF; break;
                    default: break;
                }
            }
            return;
        }

        if (offset >= 0x0400 && offset < 0x0800) {
            // VCE Registers
            uint8_t vce_sel = offset & 7;
            if (vce_sel == 2) {
                ctx->vce_addr = (ctx->vce_addr & 0x0100) | val;
            } else if (vce_sel == 3) {
                ctx->vce_addr = (ctx->vce_addr & 0x00FF) | (((uint16_t)(val & 1)) << 8);
            } else if (vce_sel == 4) {
                ctx->vce_latch = val;
            } else if (vce_sel == 5) {
                uint16_t raw_color = ((uint16_t)(val & 1) << 8) | ctx->vce_latch;
                uint16_t pal_idx = ctx->vce_addr & 511;
                ctx->vce_raw[pal_idx] = raw_color;
                uint8_t b3 = raw_color & 7;
                uint8_t r3 = (raw_color >> 3) & 7;
                uint8_t g3 = (raw_color >> 6) & 7;
                ctx->vce_palette[pal_idx] = pce_rgb_to_bgr565(r3, g3, b3);
                ctx->vce_addr = (ctx->vce_addr + 1) & 511;
            }
            return;
        }

        if (offset >= 0x0C00 && offset < 0x1000) {
            // Timer Port
            if ((offset & 1) == 0) {
                ctx->timer_reload = val & 0x7F;
            } else {
                ctx->timer_enabled = val & 1;
                if (ctx->timer_enabled) {
                    ctx->timer_counter = ctx->timer_reload;
                }
            }
            return;
        }

        if (offset >= 0x1000 && offset < 0x1400) {
            // Joypad Strobe
            ctx->joypad_select = val & 1;
            ctx->joypad_clr = val & 2;
            return;
        }

        if (offset >= 0x1400 && offset < 0x1800) {
            // Interrupt Controller
            if ((offset & 3) == 2) ctx->irq_mask = val;
            if ((offset & 3) == 3) ctx->irq_status &= ~val;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// HuC6280 CPU Execution Core
// ---------------------------------------------------------------------------
#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_T 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

static inline void set_nz(pce_context_t* ctx, uint8_t val) {
    ctx->p &= ~(FLAG_N | FLAG_Z);
    if (val == 0) ctx->p |= FLAG_Z;
    if (val & 0x80) ctx->p |= FLAG_N;
}

static inline void push(pce_context_t* ctx, uint8_t val) {
    pce_write(ctx, 0x2100 | ctx->s, val);
    ctx->s--;
}

static inline uint8_t pop(pce_context_t* ctx) {
    ctx->s++;
    return pce_read(ctx, 0x2100 | ctx->s);
}

static int pce_step_cpu(pce_context_t* ctx) {
    // Check pending interrupt request
    if ((ctx->irq_status & ~ctx->irq_mask) && !(ctx->p & FLAG_I)) {
        push(ctx, (uint8_t)(ctx->pc >> 8));
        push(ctx, (uint8_t)(ctx->pc & 0xFF));
        push(ctx, (ctx->p & ~FLAG_B) | 0x20);
        ctx->p |= FLAG_I;
        ctx->p &= ~FLAG_D;
        // Vector fetch: Timer=FFF8, VDC=FFFA
        uint16_t vector = (ctx->irq_status & 0x04) ? 0xFFF8 : 0xFFFA;
        ctx->pc = (uint16_t)pce_read(ctx, vector) | ((uint16_t)pce_read(ctx, vector + 1) << 8);
        return 7;
    }

    uint8_t op = pce_read(ctx, ctx->pc++);
    switch (op) {
        // NOP
        case 0xEA: return 2;

        // CLA, CLX, CLY (HuC6280)
        case 0x62: ctx->a = 0; return 2;
        case 0x82: ctx->x = 0; return 2;
        case 0xC2: ctx->y = 0; return 2;

        // SXY, SAX, SAY (HuC6280)
        case 0x02: { uint8_t t = ctx->x; ctx->x = ctx->y; ctx->y = t; return 3; }
        case 0x22: { uint8_t t = ctx->a; ctx->a = ctx->x; ctx->x = t; return 3; }
        case 0x42: { uint8_t t = ctx->a; ctx->a = ctx->y; ctx->y = t; return 3; }

        // ST0, ST1, ST2 (HuC6280)
        case 0x03: {
            uint8_t imm = pce_read(ctx, ctx->pc++);
            ctx->vdc_reg = imm & 0x1F;
            return 4;
        }
        case 0x13: {
            uint8_t imm = pce_read(ctx, ctx->pc++);
            ctx->vdc_latch = imm;
            return 4;
        }
        case 0x23: {
            uint8_t imm = pce_read(ctx, ctx->pc++);
            pce_write(ctx, 0x0003, imm);
            return 4;
        }

        // TAM #imm (Transfer A to MPR)
        case 0x53: {
            uint8_t mask = pce_read(ctx, ctx->pc++);
            for (int i = 0; i < 8; i++) {
                if (mask & (1 << i)) ctx->mpr[i] = ctx->a;
            }
            return 5;
        }

        // TMA #imm (Transfer MPR to A)
        case 0x73: {
            uint8_t mask = pce_read(ctx, ctx->pc++);
            for (int i = 0; i < 8; i++) {
                if (mask & (1 << i)) { ctx->a = ctx->mpr[i]; break; }
            }
            return 4;
        }

        // LDA Immediate
        case 0xA9: {
            ctx->a = pce_read(ctx, ctx->pc++);
            set_nz(ctx, ctx->a);
            return 2;
        }
        // LDA Zero Page
        case 0xA5: {
            uint8_t zp = pce_read(ctx, ctx->pc++);
            ctx->a = pce_read(ctx, 0x2000 | zp);
            set_nz(ctx, ctx->a);
            return 3;
        }
        // LDA Absolute
        case 0xAD: {
            uint16_t lo = pce_read(ctx, ctx->pc++);
            uint16_t hi = pce_read(ctx, ctx->pc++);
            ctx->a = pce_read(ctx, (hi << 8) | lo);
            set_nz(ctx, ctx->a);
            return 4;
        }

        // STA Zero Page
        case 0x85: {
            uint8_t zp = pce_read(ctx, ctx->pc++);
            pce_write(ctx, 0x2000 | zp, ctx->a);
            return 4;
        }
        // STA Absolute
        case 0x8D: {
            uint16_t lo = pce_read(ctx, ctx->pc++);
            uint16_t hi = pce_read(ctx, ctx->pc++);
            pce_write(ctx, (hi << 8) | lo, ctx->a);
            return 5;
        }

        // LDX Immediate
        case 0xA2: {
            ctx->x = pce_read(ctx, ctx->pc++);
            set_nz(ctx, ctx->x);
            return 2;
        }
        // LDY Immediate
        case 0xA0: {
            ctx->y = pce_read(ctx, ctx->pc++);
            set_nz(ctx, ctx->y);
            return 2;
        }

        // JMP Absolute
        case 0x4C: {
            uint16_t lo = pce_read(ctx, ctx->pc++);
            uint16_t hi = pce_read(ctx, ctx->pc++);
            ctx->pc = (hi << 8) | lo;
            return 4;
        }

        // JSR Absolute
        case 0x20: {
            uint16_t lo = pce_read(ctx, ctx->pc++);
            uint16_t hi = pce_read(ctx, ctx->pc++);
            uint16_t ret = ctx->pc - 1;
            push(ctx, (uint8_t)(ret >> 8));
            push(ctx, (uint8_t)(ret & 0xFF));
            ctx->pc = (hi << 8) | lo;
            return 7;
        }

        // RTS
        case 0x60: {
            uint16_t lo = pop(ctx);
            uint16_t hi = pop(ctx);
            ctx->pc = ((hi << 8) | lo) + 1;
            return 7;
        }

        // RTI
        case 0x40: {
            ctx->p = pop(ctx) | 0x20;
            uint16_t lo = pop(ctx);
            uint16_t hi = pop(ctx);
            ctx->pc = (hi << 8) | lo;
            return 7;
        }

        // SEI, CLI
        case 0x78: ctx->p |= FLAG_I; return 2;
        case 0x58: ctx->p &= ~FLAG_I; return 2;

        // CLD, SED
        case 0xD8: ctx->p &= ~FLAG_D; return 2;
        case 0xF8: ctx->p |= FLAG_D; return 2;

        // INX, DEX, INY, DEY
        case 0xE8: ctx->x++; set_nz(ctx, ctx->x); return 2;
        case 0xCA: ctx->x--; set_nz(ctx, ctx->x); return 2;
        case 0xC8: ctx->y++; set_nz(ctx, ctx->y); return 2;
        case 0x88: ctx->y--; set_nz(ctx, ctx->y); return 2;

        // Branches
        case 0xF0: { // BEQ
            int8_t rel = (int8_t)pce_read(ctx, ctx->pc++);
            if (ctx->p & FLAG_Z) { ctx->pc += rel; return 4; }
            return 2;
        }
        case 0xD0: { // BNE
            int8_t rel = (int8_t)pce_read(ctx, ctx->pc++);
            if (!(ctx->p & FLAG_Z)) { ctx->pc += rel; return 4; }
            return 2;
        }
        case 0x80: { // BRA (65C02)
            int8_t rel = (int8_t)pce_read(ctx, ctx->pc++);
            ctx->pc += rel;
            return 4;
        }

        default:
            // Fallback for unhandled opcodes: advance PC and return default cycles
            return 2;
    }
}

// ---------------------------------------------------------------------------
// VDC Scanline Compositor (Background & Sprites)
// ---------------------------------------------------------------------------
static void pce_render_scanline(pce_context_t* ctx, int scanline) {
    if (!ctx || !ctx->framebuffer || scanline < 0 || scanline >= 240) return;

    uint16_t* line_buf = &ctx->framebuffer[scanline * 256];
    uint16_t bg_color = ctx->vce_palette[0];

    // Background Render Pass
    if (ctx->vdc_cr & 0x80) { // BG Enable
        int v_scroll = (scanline + ctx->vdc_byr) & 511;
        int tile_y = (v_scroll / 8) & 31; // 32 rows default
        int fine_y = v_scroll & 7;

        for (int col = 0; col < 32; col++) {
            int h_scroll = (col * 8 + ctx->vdc_bxr) & 511;
            int tile_x = (h_scroll / 8) & 31; // 32 cols default
            int fine_x = h_scroll & 7;

            // BAT entry lookup in VRAM
            uint32_t bat_addr = (tile_y * 32 + tile_x) * 2;
            uint16_t bat = ctx->vram[bat_addr] | ((uint16_t)ctx->vram[bat_addr + 1] << 8);

            uint16_t pattern_name = bat & 0x0FFF;
            uint8_t pal_group = (bat >> 12) & 0x0F;

            // 8x8 Tile: 4 bitplanes (32 bytes per tile)
            uint32_t pat_addr = (pattern_name * 32) & 0xFFFF;
            uint8_t p0 = ctx->vram[pat_addr + fine_y * 2];
            uint8_t p1 = ctx->vram[pat_addr + fine_y * 2 + 1];
            uint8_t p2 = ctx->vram[pat_addr + 16 + fine_y * 2];
            uint8_t p3 = ctx->vram[pat_addr + 16 + fine_y * 2 + 1];

            for (int px = 0; px < 8; px++) {
                int out_x = col * 8 + px - fine_x;
                if (out_x >= 0 && out_x < 256) {
                    int bit = 7 - px;
                    uint8_t c = (((p3 >> bit) & 1) << 3)
                              | (((p2 >> bit) & 1) << 2)
                              | (((p1 >> bit) & 1) << 1)
                              |  ((p0 >> bit) & 1);

                    if (c != 0) {
                        uint16_t color_idx = (pal_group << 4) | c;
                        line_buf[out_x] = ctx->vce_palette[color_idx & 511];
                    } else {
                        line_buf[out_x] = bg_color;
                    }
                }
            }
        }
    } else {
        // Background disabled: clear to backdrop
        for (int x = 0; x < 256; x++) {
            line_buf[x] = bg_color;
        }
    }

    // Sprite Render Pass (SATB)
    if (ctx->vdc_cr & 0x40) { // Sprite Enable
        uint32_t satb_base = (ctx->vdc_satb * 2) & 0xFFFF;
        for (int spr = 63; spr >= 0; spr--) {
            uint32_t s_addr = satb_base + spr * 8;
            int sy = (ctx->vram[s_addr] | ((uint16_t)ctx->vram[s_addr + 1] << 8)) - 64;
            int sx = (ctx->vram[s_addr + 2] | ((uint16_t)ctx->vram[s_addr + 3] << 8)) - 32;

            if (scanline >= sy && scanline < sy + 16) {
                uint16_t pat = ctx->vram[s_addr + 4] | ((uint16_t)ctx->vram[s_addr + 5] << 8);
                uint16_t attr = ctx->vram[s_addr + 6] | ((uint16_t)ctx->vram[s_addr + 7] << 8);
                uint8_t pal_group = (attr & 0x0F) + 16; // Sprite palettes 16..31

                int spr_y = scanline - sy;
                uint32_t spr_pat_addr = ((pat >> 1) * 128) & 0xFFFF;
                uint8_t sp0 = ctx->vram[spr_pat_addr + spr_y * 2];
                uint8_t sp1 = ctx->vram[spr_pat_addr + spr_y * 2 + 1];

                for (int spx = 0; spx < 16; spx++) {
                    int out_x = sx + spx;
                    if (out_x >= 0 && out_x < 256) {
                        int bit = 7 - (spx & 7);
                        uint8_t sc = (((sp1 >> bit) & 1) << 1) | ((sp0 >> bit) & 1);
                        if (sc != 0) {
                            uint16_t pal_idx = (pal_group << 4) | sc;
                            line_buf[out_x] = ctx->vce_palette[pal_idx & 511];
                        }
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Public Core Lifecycle APIs
// ---------------------------------------------------------------------------
pce_context_t* pce_create(void) {
    pce_context_t* ctx = (pce_context_t*)PCE_MALLOC(sizeof(pce_context_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(pce_context_t));

    // Allocate 256x240 16-bit framebuffer in Octal PSRAM
    ctx->framebuffer = (uint16_t*)PCE_MALLOC(256 * 240 * sizeof(uint16_t));
    if (!ctx->framebuffer) {
        PCE_FREE(ctx);
        return NULL;
    }

    // Default VCE Palette initialization
    for (int i = 0; i < 512; i++) {
        uint8_t b = (uint8_t)((i & 7) * 36);
        uint8_t r = (uint8_t)(((i >> 3) & 7) * 36);
        uint8_t g = (uint8_t)(((i >> 6) & 7) * 36);
        ctx->vce_raw[i] = (uint16_t)i;
        ctx->vce_palette[i] = pce_rgb_to_bgr565(r, g, b);
    }
    return ctx;
}

void pce_destroy(pce_context_t* ctx) {
    if (ctx) {
        if (ctx->framebuffer) {
            PCE_FREE(ctx->framebuffer);
            ctx->framebuffer = NULL;
        }
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
        ctx->mpr[i] = (uint8_t)i;
    }
    ctx->mpr[7] = 0xF8;

    // CPU Reset Vector (0xFFFE - 0xFFFF in page 7)
    ctx->pc = (size >= 2) ? (uint16_t)(rom[size - 2] | (rom[size - 1] << 8)) : 0xE000;
    ctx->s = 0xFF;
    ctx->p = 0x20;
    ctx->joypad = 0xFF;
    ctx->vdc_cr = 0x00C0; // Default BG and Sprite enable

    return true;
}

void pce_set_input(pce_context_t* ctx, uint8_t joypad) {
    if (ctx) {
        ctx->joypad = joypad;
    }
}

void pce_run_frame(pce_context_t* ctx) {
    if (!ctx || !ctx->framebuffer) return;

    // 262 scanlines per frame (240 visible + 22 VBlank)
    for (int line = 0; line < 262; line++) {
        // Step CPU for ~455 cycles per scanline (7.16 MHz / 60 Hz / 262 lines)
        int cycles = 0;
        while (cycles < 455) {
            cycles += pce_step_cpu(ctx);
        }

        // Timer update
        if (ctx->timer_enabled) {
            if (ctx->timer_counter == 0) {
                ctx->timer_counter = ctx->timer_reload;
                ctx->irq_status |= 0x04; // Assert Timer IRQ2
            } else {
                ctx->timer_counter--;
            }
        }

        // Raster Compare Interrupt
        if (line == ctx->vdc_rcr && (ctx->vdc_cr & 0x02)) {
            ctx->vdc_status |= 0x04;
            ctx->irq_status |= 0x02; // Assert VDC IRQ1
        }

        // Render visible scanline
        if (line < 240) {
            pce_render_scanline(ctx, line);
        }

        // VBlank Interrupt entry
        if (line == 240) {
            ctx->vdc_status |= 0x20; // Set VBlank flag
            if (ctx->vdc_cr & 0x01) {
                ctx->irq_status |= 0x02; // Assert VBlank IRQ1
            }
        }
    }
}
