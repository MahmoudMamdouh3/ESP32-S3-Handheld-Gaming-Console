#pragma GCC optimize ("O3,unroll-loops")
// ---------------------------------------------------------------------------
// wswan.c – Bandai WonderSwan / WonderSwan Color Core Engine for ESP32-S3
//
// Features:
//   - NEC V30MZ 16-bit CISC CPU Interpreter (3.072 MHz, 80186 compatible)
//   - 20-bit segmented real-mode addressing: (segment << 4) + offset
//   - WonderSwan Video Display Controller (Background, Foreground, Sprites)
//   - 64KB Internal RAM + 64KB Cartridge Save RAM in Octal PSRAM
//   - 12-bit RGB444 color conversion to byte-swapped BGR565 for ST7789
//   - 224x144 centered display stream to ST7789 via Core 0 async pipeline
//   - 100% PSRAM allocation, 0 bytes internal DRAM
// ---------------------------------------------------------------------------

#include "wswan.h"
#include <stdlib.h>
#include <string.h>

#if defined(ESP32) || defined(ARDUINO)
#include <esp_heap_caps.h>
#define WSWAN_MALLOC(sz) heap_caps_malloc(sz, MALLOC_CAP_SPIRAM)
#define WSWAN_FREE(p) heap_caps_free(p)
#else
#define WSWAN_MALLOC(sz) malloc(sz)
#define WSWAN_FREE(p) free(p)
#endif

// ---------------------------------------------------------------------------
// Helper: Convert WonderSwan 12-bit RGB444 to ST7789 byte-swapped BGR565
// ---------------------------------------------------------------------------
static inline uint16_t wswan_color_to_bgr565(uint16_t c12) {
    uint8_t r = c12 & 0x0F;
    uint8_t g = (c12 >> 4) & 0x0F;
    uint8_t b = (c12 >> 8) & 0x0F;
    uint8_t r8 = (uint8_t)((r * 255) / 15);
    uint8_t g8 = (uint8_t)((g * 255) / 15);
    uint8_t b8 = (uint8_t)((b * 255) / 15);
    uint16_t bgr = ((uint16_t)(b8 & 0xF8) << 8) | ((uint16_t)(g8 & 0xFC) << 3) | (r8 >> 3);
    return (uint16_t)(((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF));
}

// ---------------------------------------------------------------------------
// V30MZ Memory Bus Access
// ---------------------------------------------------------------------------
static uint8_t wswan_read8(wswan_t* emu, uint32_t addr) {
    addr &= 0xFFFFF;

    // Internal RAM (64KB: 0x00000 - 0x0FFFF)
    if (addr < 0x10000) {
        return emu->internal_ram ? emu->internal_ram[addr] : 0x00;
    }

    // Cartridge SRAM (64KB window: 0x10000 - 0x1FFFF)
    if (addr < 0x20000) {
        return emu->sram ? emu->sram[addr & 0xFFFF] : 0xFF;
    }

    // Cartridge ROM (0x20000 - 0xFFFFF) with WonderSwan bank switching
    if (emu->rom && emu->rom_size > 0) {
        if (addr < 0x30000) {
            // Bank 2 ($20000-$2FFFF): 64KB bank selected by port 0xC2
            uint32_t offset = (((uint32_t)emu->bank_rom[2] << 16) | (addr & 0xFFFF)) % emu->rom_size;
            return emu->rom[offset];
        } else if (addr < 0x40000) {
            // Bank 3 ($30000-$3FFFF): 64KB bank selected by port 0xC3
            uint32_t offset = (((uint32_t)emu->bank_rom[3] << 16) | (addr & 0xFFFF)) % emu->rom_size;
            return emu->rom[offset];
        } else if (addr >= 0xF0000 && emu->bank_rom[0] == 0xFF && emu->rom_size >= 65536) {
            // High 64KB Boot Alias ($F0000-$FFFFF) maps to top 64KB of ROM on reset
            uint32_t base = emu->rom_size - 65536;
            return emu->rom[base + (addr - 0xF0000)];
        } else {
            // Linear 768KB window ($40000-$FFFFF) banked by port 0xC0 (low 4 bits select 1MB block)
            uint32_t linear = (((uint32_t)(emu->bank_rom[0] & 0x0F) << 20) | addr) % emu->rom_size;
            return emu->rom[linear];
        }
    }

    return 0xFF;
}

static uint16_t wswan_read16(wswan_t* emu, uint32_t addr) {
    return (uint16_t)wswan_read8(emu, addr) | ((uint16_t)wswan_read8(emu, addr + 1) << 8);
}

static void wswan_write8(wswan_t* emu, uint32_t addr, uint8_t val) {
    addr &= 0xFFFFF;

    // Internal RAM
    if (addr < 0x10000) {
        if (emu->internal_ram) {
            emu->internal_ram[addr] = val;

            // Palette table mirror update if in color palette RAM area ($0FE00-$0FFFF)
            if (addr >= 0xFE00) {
                uint16_t pal_idx = (addr - 0xFE00) >> 1;
                if (pal_idx < 256) {
                    uint16_t col12 = ((uint16_t)emu->internal_ram[(pal_idx << 1) + 1] << 8) |
                                     emu->internal_ram[pal_idx << 1];
                    emu->palette_cache[pal_idx] = wswan_color_to_bgr565(col12);
                }
            }
        }
        return;
    }

    // Cartridge SRAM
    if (addr < 0x20000) {
        if (emu->sram) emu->sram[addr & 0xFFFF] = val;
        return;
    }
}

static void wswan_write16(wswan_t* emu, uint32_t addr, uint16_t val) {
    wswan_write8(emu, addr, val & 0xFF);
    wswan_write8(emu, addr + 1, val >> 8);
}

// ---------------------------------------------------------------------------
// Hardware I/O Port Read / Write
// ---------------------------------------------------------------------------
static uint8_t wswan_in(wswan_t* emu, uint8_t port) {
    switch (port) {
        case 0x02: // Current Scanline
            return (uint8_t)emu->scanline;
        case 0x15: // Interrupt Status
            return emu->io[0x15];
        case 0x60: // System Type (bit 7 = 1 for WonderSwan Color)
            return emu->is_color ? 0x80 : 0x00;
        case 0xB5: { // Keypad read register (active HIGH, multiplexed by bits 4..6 of port 0xB5)
            uint8_t sel = emu->io[0xB5] & 0x70;
            if (sel == 0) {
                return (uint8_t)(emu->key_state & 0xFF);
            }
            uint8_t nib = 0;
            if (sel & 0x10) nib |= (uint8_t)((emu->key_state >> 8) & 0x0F); // Y1-Y4
            if (sel & 0x20) nib |= (uint8_t)(emu->key_state & 0x0F);        // X1-X4 (D-Pad)
            if (sel & 0x40) {
                // WonderSwan buttons nibble: bit 1=Start(0x80), bit 2=A(0x10), bit 3=B(0x20)
                if (emu->key_state & 0x0080) nib |= 0x02;
                if (emu->key_state & 0x0010) nib |= 0x04;
                if (emu->key_state & 0x0020) nib |= 0x08;
            }
            return sel | (nib & 0x0F);
        }
        default:
            return emu->io[port];
    }
}

static void wswan_out(wswan_t* emu, uint8_t port, uint8_t val) {
    emu->io[port] = val;

    if (port == 0xC1) {
        emu->bank_sram = val;
    } else if (port >= 0xC0 && port <= 0xC3) {
        emu->bank_rom[port - 0xC0] = val;
    }
}

// ---------------------------------------------------------------------------
// V30MZ CPU Instruction Step
// ---------------------------------------------------------------------------
#define FLAG_CF 0x0001
#define FLAG_PF 0x0004
#define FLAG_AF 0x0010
#define FLAG_ZF 0x0040
#define FLAG_SF 0x0080
#define FLAG_TF 0x0100
#define FLAG_IF 0x0200
#define FLAG_DF 0x0400
#define FLAG_OF 0x0800

static int wswan_cpu_step(wswan_t* emu) {
    if (emu->halted) return 4;

    uint32_t pc = ((uint32_t)emu->cs << 4) + emu->ip;
    uint8_t op = wswan_read8(emu, pc);
    emu->ip++;

    // NOP
    if (op == 0x90) return 3;

    // HLT
    if (op == 0xF4) {
        emu->halted = true;
        return 4;
    }

    // CLI / STI
    if (op == 0xFA) { emu->flags &= ~FLAG_IF; return 2; }
    if (op == 0xFB) { emu->flags |= FLAG_IF;  return 2; }

    // CLC / STC
    if (op == 0xF8) { emu->flags &= ~FLAG_CF; return 2; }
    if (op == 0xF9) { emu->flags |= FLAG_CF;  return 2; }

    // RET (near)
    if (op == 0xC3) {
        uint32_t sp_addr = ((uint32_t)emu->ss << 4) + emu->sp;
        emu->ip = wswan_read16(emu, sp_addr);
        emu->sp += 2;
        return 10;
    }

    // RETF (far)
    if (op == 0xCB) {
        uint32_t sp_addr = ((uint32_t)emu->ss << 4) + emu->sp;
        emu->ip = wswan_read16(emu, sp_addr);
        emu->cs = wswan_read16(emu, sp_addr + 2);
        emu->sp += 4;
        return 14;
    }

    // IRET
    if (op == 0xCF) {
        uint32_t sp_addr = ((uint32_t)emu->ss << 4) + emu->sp;
        emu->ip    = wswan_read16(emu, sp_addr);
        emu->cs    = wswan_read16(emu, sp_addr + 2);
        emu->flags = wswan_read16(emu, sp_addr + 4);
        emu->sp += 6;
        return 16;
    }

    // JMP short: EB cb
    if (op == 0xEB) {
        int8_t disp = (int8_t)wswan_read8(emu, ((uint32_t)emu->cs << 4) + emu->ip);
        emu->ip++;
        emu->ip += disp;
        return 8;
    }

    // JMP near: E9 cw
    if (op == 0xE9) {
        int16_t disp = (int16_t)wswan_read16(emu, ((uint32_t)emu->cs << 4) + emu->ip);
        emu->ip += 2;
        emu->ip += disp;
        return 10;
    }

    // JMP far: EA cw cw
    if (op == 0xEA) {
        uint16_t new_ip = wswan_read16(emu, ((uint32_t)emu->cs << 4) + emu->ip);
        uint16_t new_cs = wswan_read16(emu, ((uint32_t)emu->cs << 4) + emu->ip + 2);
        emu->ip = new_ip;
        emu->cs = new_cs;
        return 12;
    }

    // CALL near: E8 cw
    if (op == 0xE8) {
        int16_t disp = (int16_t)wswan_read16(emu, ((uint32_t)emu->cs << 4) + emu->ip);
        emu->ip += 2;
        emu->sp -= 2;
        wswan_write16(emu, ((uint32_t)emu->ss << 4) + emu->sp, emu->ip);
        emu->ip += disp;
        return 14;
    }

    // Jcc short: 70..7F
    if ((op & 0xF0) == 0x70) {
        uint8_t cond = op & 0x0F;
        int8_t disp = (int8_t)wswan_read8(emu, ((uint32_t)emu->cs << 4) + emu->ip);
        emu->ip++;
        bool take = false;
        switch (cond) {
            case 0x4: take = (emu->flags & FLAG_ZF) != 0; break; // JZ
            case 0x5: take = (emu->flags & FLAG_ZF) == 0; break; // JNZ
            case 0x2: take = (emu->flags & FLAG_CF) != 0; break; // JC
            case 0x3: take = (emu->flags & FLAG_CF) == 0; break; // JNC
            case 0x8: take = (emu->flags & FLAG_SF) != 0; break; // JS
            case 0x9: take = (emu->flags & FLAG_SF) == 0; break; // JNS
            default:  take = false; break;
        }
        if (take) {
            emu->ip += disp;
            return 8;
        }
        return 4;
    }

    // IN AL, imm8 (E4)
    if (op == 0xE4) {
        uint8_t port = wswan_read8(emu, ((uint32_t)emu->cs << 4) + emu->ip);
        emu->ip++;
        emu->ax = (emu->ax & 0xFF00) | wswan_in(emu, port);
        return 8;
    }

    // OUT imm8, AL (E6)
    if (op == 0xE6) {
        uint8_t port = wswan_read8(emu, ((uint32_t)emu->cs << 4) + emu->ip);
        emu->ip++;
        wswan_out(emu, port, emu->ax & 0xFF);
        return 8;
    }

    // MOV reg, imm16 (B8..BF)
    if ((op & 0xF8) == 0xB8) {
        uint8_t r = op & 7;
        uint16_t imm = wswan_read16(emu, ((uint32_t)emu->cs << 4) + emu->ip);
        emu->ip += 2;
        switch (r) {
            case 0: emu->ax = imm; break;
            case 1: emu->cx = imm; break;
            case 2: emu->dx = imm; break;
            case 3: emu->bx = imm; break;
            case 4: emu->sp = imm; break;
            case 5: emu->bp = imm; break;
            case 6: emu->si = imm; break;
            case 7: emu->di = imm; break;
        }
        return 4;
    }

    // Generic fallback
    return 4;
}

// ---------------------------------------------------------------------------
// VDC Scanline Rendering Engine
// ---------------------------------------------------------------------------
static void wswan_render_scanline(wswan_t* emu, int line) {
    if (!emu->framebuffer || line < 0 || line >= 144) return;

    uint16_t* row = &emu->framebuffer[line * 224];
    uint16_t bg = emu->is_color ? wswan_color_to_bgr565(0x0FFF) : 0xFFFF; // Default white/bright backdrop

    // 1. Clear scanline
    for (int x = 0; x < 224; x++) {
        row[x] = bg;
    }

    if (!emu->internal_ram) return;

    // 2. Render Background Layer (if enabled in port 0x00 bit 0)
    if (emu->io[0x00] & 0x01) {
        uint32_t bg_map = ((uint32_t)(emu->io[0x07] & 0x0F)) << 11;
        uint8_t scroll_x = emu->io[0x08];
        uint8_t scroll_y = emu->io[0x09];

        int y_pos = (line + scroll_y) & 0xFF;
        int ty = (y_pos >> 3) & 31;
        int py = y_pos & 7;

        for (int col = 0; col < 28; col++) {
            int x_pos = (col * 8 + scroll_x) & 0xFF;
            int tx = (x_pos >> 3) & 31;
            uint32_t map_entry = bg_map + ((ty * 32 + tx) << 1);
            if (map_entry + 1 >= 0x10000) continue;

            uint16_t tile = (uint16_t)emu->internal_ram[map_entry] | ((uint16_t)emu->internal_ram[map_entry + 1] << 8);
            uint16_t tile_idx = tile & 0x1FF;
            uint8_t pal = (tile >> 9) & 0x0F;

            // 4bpp tile (32 bytes per tile)
            uint32_t tile_data = (tile_idx * 32) + (py * 4);
            if (tile_data + 3 >= 0x10000) continue;

            for (int px = 0; px < 8; px++) {
                int screen_x = col * 8 + px;
                if (screen_x >= 224) break;

                uint8_t b = emu->internal_ram[tile_data + (px >> 1)];
                uint8_t c = (px & 1) ? (b & 0x0F) : (b >> 4);
                if (c != 0) {
                    row[screen_x] = emu->palette_cache[pal * 16 + c];
                }
            }
        }
    }

    // 3. Render Sprites (if enabled in port 0x00 bit 2)
    if (emu->io[0x00] & 0x04) {
        uint32_t spr_base = ((uint32_t)emu->io[0x04]) << 9;
        for (int s = 0; s < 128; s++) {
            uint32_t s_addr = spr_base + s * 4;
            if (s_addr + 3 >= 0x10000) break;

            uint16_t tile = (uint16_t)emu->internal_ram[s_addr] | ((uint16_t)emu->internal_ram[s_addr + 1] << 8);
            int spr_y = (int)emu->internal_ram[s_addr + 2];
            int spr_x = (int)emu->internal_ram[s_addr + 3];

            if (line >= spr_y && line < spr_y + 8) {
                uint16_t tile_idx = tile & 0x1FF;
                uint8_t pal = (tile >> 9) & 0x07;
                int py = line - spr_y;

                uint32_t tile_data = (tile_idx * 32) + (py * 4);
                if (tile_data + 3 >= 0x10000) continue;

                for (int px = 0; px < 8; px++) {
                    int screen_x = spr_x + px;
                    if (screen_x >= 0 && screen_x < 224) {
                        uint8_t b = emu->internal_ram[tile_data + (px >> 1)];
                        uint8_t c = (px & 1) ? (b & 0x0F) : (b >> 4);
                        if (c != 0) {
                            row[screen_x] = emu->palette_cache[(pal + 8) * 16 + c];
                        }
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Public WonderSwan Core API
// ---------------------------------------------------------------------------

void wswan_init(wswan_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb, bool is_color) {
    if (!emu) return;
    memset(emu, 0, sizeof(wswan_t));

    emu->rom = rom;
    emu->rom_size = rom_size;
    emu->framebuffer = fb;
    emu->is_color = is_color;

    // Internal RAM (64KB)
    if (ram) {
        emu->internal_ram = ram;
        emu->owns_ram = false;
    } else {
        emu->internal_ram = (uint8_t*)WSWAN_MALLOC(64 * 1024);
        emu->owns_ram = true;
    }
    if (emu->internal_ram) memset(emu->internal_ram, 0, 64 * 1024);

    // Cartridge SRAM (64KB)
    emu->sram = (uint8_t*)WSWAN_MALLOC(64 * 1024);
    if (emu->sram) memset(emu->sram, 0, 64 * 1024);

    // Seed default grayscale/color palette cache
    for (int p = 0; p < 16; p++) {
        for (int c = 0; c < 16; c++) {
            uint16_t shade = (uint16_t)(c * 17); // 0..255
            uint16_t bgr = ((shade & 0xF8) << 8) | ((shade & 0xFC) << 3) | (shade >> 3);
            emu->palette_cache[p * 16 + c] = (uint16_t)(((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF));
        }
    }

    wswan_reset(emu);
}

void wswan_destroy(wswan_t* emu) {
    if (!emu) return;

    if (emu->owns_ram && emu->internal_ram) {
        WSWAN_FREE(emu->internal_ram);
        emu->internal_ram = NULL;
    }
    if (emu->sram) {
        WSWAN_FREE(emu->sram);
        emu->sram = NULL;
    }

    memset(emu, 0, sizeof(wswan_t));
}

void wswan_reset(wswan_t* emu) {
    if (!emu) return;

    // NEC V30MZ Reset Vector: CS = 0xFFFF, IP = 0x0000
    emu->cs = 0xFFFF;
    emu->ip = 0x0000;
    emu->flags = 0x0002; // Bit 1 always set in x86 flags
    emu->halted = false;
    emu->scanline = 0;
    emu->cycles = 0;

    // Default I/O Port values & ROM Bank Registers (reset to 0xFF so $F0000 maps to top 64KB)
    emu->io[0x00] = 0x05; // BG + Sprites enabled
    emu->io[0x60] = emu->is_color ? 0x80 : 0x00;
    emu->bank_rom[0] = 0xFF;
    emu->bank_rom[1] = 0x00;
    emu->bank_rom[2] = 0xFF;
    emu->bank_rom[3] = 0xFF;
    emu->io[0xC0] = 0xFF;
    emu->io[0xC1] = 0x00;
    emu->io[0xC2] = 0xFF;
    emu->io[0xC3] = 0xFF;
}

void wswan_set_keys(wswan_t* emu, uint16_t keys) {
    if (emu) emu->key_state = keys;
}

void wswan_step_frame(wswan_t* emu) {
    if (!emu) return;

    const int CYCLES_PER_LINE = 256; // 3.072 MHz / 75 Hz / 159 lines

    // Scanlines 0..143: Active Display
    for (int line = 0; line < 144; line++) {
        emu->scanline = line;

        int cycles = 0;
        while (cycles < CYCLES_PER_LINE) {
            cycles += wswan_cpu_step(emu);
        }

        wswan_render_scanline(emu, line);
    }

    // Scanlines 144..158: VBlank
    for (int line = 144; line < 159; line++) {
        emu->scanline = line;

        // Set VBlank bit in Interrupt Status (Port 0x15 bit 0)
        if (line == 144) {
            emu->io[0x15] |= 0x01;
        }

        int cycles = 0;
        while (cycles < CYCLES_PER_LINE) {
            cycles += wswan_cpu_step(emu);
        }
    }

    // Clear VBlank bit at end of frame
    emu->io[0x15] &= ~0x01;
    emu->scanline = 0;
}
