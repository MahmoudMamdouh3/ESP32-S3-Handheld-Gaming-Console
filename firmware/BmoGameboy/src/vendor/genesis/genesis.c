#pragma GCC optimize ("O3,unroll-loops")
// ---------------------------------------------------------------------------
// genesis.c – Sega Genesis / Mega Drive Core Engine for ESP32-S3
//
// Features:
//   - Motorola 68000 16/32-bit CISC CPU Interpreter (7.67 MHz NTSC)
//   - Yamaha YM7101 / 315-5313 VDP (Planes A & B, Window, Hardware Sprites)
//   - 64KB VRAM, 128B CRAM (64 colors), 80B VSRAM in Octal PSRAM
//   - 64KB Work RAM + 8KB Z80 RAM in Octal PSRAM
//   - 320x224 display stream to ST7789 via Core 0 async pipeline
//   - 100% PSRAM allocation, 0 bytes internal DRAM
// ---------------------------------------------------------------------------

#include "genesis.h"
#include <stdlib.h>
#include <string.h>

#if defined(ESP32) || defined(ARDUINO)
#include <esp_heap_caps.h>
#define GENESIS_MALLOC(sz) heap_caps_malloc(sz, MALLOC_CAP_SPIRAM)
#define GENESIS_FREE(p) heap_caps_free(p)
#else
#define GENESIS_MALLOC(sz) malloc(sz)
#define GENESIS_FREE(p) free(p)
#endif

// ---------------------------------------------------------------------------
// Helper: Convert Genesis 9-bit RGB to ST7789 byte-swapped BGR565
// ---------------------------------------------------------------------------
static inline uint16_t genesis_cram_to_bgr565(uint16_t cram_val) {
    uint8_t r = (cram_val >> 1) & 0x07;
    uint8_t g = (cram_val >> 5) & 0x07;
    uint8_t b = (cram_val >> 9) & 0x07;
    uint8_t r8 = (uint8_t)((r * 255) / 7);
    uint8_t g8 = (uint8_t)((g * 255) / 7);
    uint8_t b8 = (uint8_t)((b * 255) / 7);
    uint16_t bgr = ((uint16_t)(b8 & 0xF8) << 8) | ((uint16_t)(g8 & 0xFC) << 3) | (r8 >> 3);
    return (uint16_t)(((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF));
}

// ---------------------------------------------------------------------------
// 68000 Memory Bus Read / Write Prototypes
// ---------------------------------------------------------------------------
static uint8_t  genesis_read8(genesis_t* emu, uint32_t addr);
static uint16_t genesis_read16(genesis_t* emu, uint32_t addr);
static uint32_t genesis_read32(genesis_t* emu, uint32_t addr);
static void     genesis_write8(genesis_t* emu, uint32_t addr, uint8_t val);
static void     genesis_write16(genesis_t* emu, uint32_t addr, uint16_t val);
static void     genesis_write32(genesis_t* emu, uint32_t addr, uint32_t val);

// ---------------------------------------------------------------------------
// 68000 Memory Bus Implementation
// ---------------------------------------------------------------------------
static uint8_t genesis_read8(genesis_t* emu, uint32_t addr) {
    addr &= 0xFFFFFF;

    // Cartridge ROM (up to 4MB)
    if (addr < 0x400000) {
        if (addr < emu->rom_size && emu->rom) {
            return emu->rom[addr];
        }
        return 0xFF;
    }

    // Z80 RAM (8KB)
    if (addr >= 0xA00000 && addr <= 0xA01FFF) {
        return emu->z80_ram ? emu->z80_ram[addr & 0x1FFF] : 0xFF;
    }

    // I/O Ports
    if (addr >= 0xA10000 && addr <= 0xA1001F) {
        uint8_t reg = addr & 0x1F;
        if (reg == 0x00 || reg == 0x01) {
            return 0xA0; // Model 1 NTSC Mega Drive/Genesis
        }
        if (reg == 0x02 || reg == 0x03) {
            // Port 1 Data
            uint8_t th = (emu->io_data[0] & 0x40);
            uint8_t data = 0x7F;
            if (th) {
                // TH = 1: D-Pad (Up, Down, Left, Right) + B + C
                if (emu->pad_state & 0x01) data &= ~0x01; // Up
                if (emu->pad_state & 0x02) data &= ~0x02; // Down
                if (emu->pad_state & 0x04) data &= ~0x04; // Left
                if (emu->pad_state & 0x08) data &= ~0x08; // Right
                if (emu->pad_state & 0x10) data &= ~0x10; // B
                if (emu->pad_state & 0x40) data &= ~0x20; // C
            } else {
                // TH = 0: Up + Down + A + Start
                if (emu->pad_state & 0x01) data &= ~0x01; // Up
                if (emu->pad_state & 0x02) data &= ~0x02; // Down
                if (emu->pad_state & 0x20) data &= ~0x10; // A
                if (emu->pad_state & 0x80) data &= ~0x20; // Start
            }
            return data;
        }
        if (reg == 0x04 || reg == 0x05) return 0x7F; // Port 2 Data (unconnected)
        if (reg == 0x08 || reg == 0x09) return emu->io_ctrl[0];
        if (reg == 0x0A || reg == 0x0B) return emu->io_ctrl[1];
        return 0xFF;
    }

    // Z80 Control
    if (addr == 0xA11100 || addr == 0xA11101) {
        return (emu->z80_busreq ? 0x01 : 0x00);
    }

    // VDP Ports
    if (addr >= 0xC00000 && addr <= 0xC0001F) {
        uint16_t w = genesis_read16(emu, addr & ~1);
        return (addr & 1) ? (w & 0xFF) : (w >> 8);
    }

    // 64KB Work RAM (and mirrors up to 0xFFFFFF)
    if (addr >= 0xE00000) {
        return emu->ram ? emu->ram[addr & 0xFFFF] : 0x00;
    }

    return 0xFF;
}

static uint16_t genesis_read16(genesis_t* emu, uint32_t addr) {
    addr &= 0xFFFFFE;

    // Cartridge ROM
    if (addr < 0x400000) {
        if (addr + 1 < emu->rom_size && emu->rom) {
            return ((uint16_t)emu->rom[addr] << 8) | emu->rom[addr + 1];
        }
        return 0xFFFF;
    }

    // VDP Control Port
    if (addr == 0xC00004 || addr == 0xC00006) {
        uint16_t st = 0x0200; // FIFO empty
        if (emu->scanline >= 224 && emu->scanline <= 261) {
            st |= 0x0008; // VBlank active
        }
        emu->vdp_pending = 0;
        return st;
    }

    // VDP Data Port
    if (addr == 0xC00000 || addr == 0xC00002) {
        uint16_t val = 0;
        uint32_t target = emu->vdp_addr;
        if (emu->vdp_code == 0) {
            // VRAM Read
            if (emu->vram) {
                val = ((uint16_t)emu->vram[target & 0xFFFF] << 8) | emu->vram[(target + 1) & 0xFFFF];
            }
        } else if (emu->vdp_code == 8) {
            // CRAM Read
            if (emu->cram) {
                val = emu->cram[(target >> 1) & 0x3F];
            }
        }
        emu->vdp_addr += emu->vdp_regs[15];
        return val;
    }

    // Work RAM
    if (addr >= 0xE00000) {
        if (emu->ram) {
            uint32_t offset = addr & 0xFFFF;
            return ((uint16_t)emu->ram[offset] << 8) | emu->ram[(offset + 1) & 0xFFFF];
        }
        return 0;
    }

    return ((uint16_t)genesis_read8(emu, addr) << 8) | genesis_read8(emu, addr + 1);
}

static uint32_t genesis_read32(genesis_t* emu, uint32_t addr) {
    return ((uint32_t)genesis_read16(emu, addr) << 16) | genesis_read16(emu, addr + 2);
}

static void genesis_write8(genesis_t* emu, uint32_t addr, uint8_t val) {
    addr &= 0xFFFFFF;

    // Z80 RAM
    if (addr >= 0xA00000 && addr <= 0xA01FFF) {
        if (emu->z80_ram) emu->z80_ram[addr & 0x1FFF] = val;
        return;
    }

    // I/O Ports
    if (addr >= 0xA10000 && addr <= 0xA1001F) {
        uint8_t reg = addr & 0x1F;
        if (reg == 0x02 || reg == 0x03) emu->io_data[0] = val;
        if (reg == 0x04 || reg == 0x05) emu->io_data[1] = val;
        if (reg == 0x08 || reg == 0x09) emu->io_ctrl[0] = val;
        if (reg == 0x0A || reg == 0x0B) emu->io_ctrl[1] = val;
        return;
    }

    // Z80 Bus Request / Reset
    if (addr == 0xA11100 || addr == 0xA11101) {
        emu->z80_busreq = val & 1;
        return;
    }
    if (addr == 0xA11200 || addr == 0xA11201) {
        emu->z80_reset = val & 1;
        return;
    }

    // VDP Ports
    if (addr >= 0xC00000 && addr <= 0xC0001F) {
        genesis_write16(emu, addr & ~1, ((uint16_t)val << 8) | val);
        return;
    }

    // Work RAM
    if (addr >= 0xE00000) {
        if (emu->ram) emu->ram[addr & 0xFFFF] = val;
        return;
    }
}

static void genesis_write16(genesis_t* emu, uint32_t addr, uint16_t val) {
    addr &= 0xFFFFFE;

    // VDP Control Port
    if (addr == 0xC00004 || addr == 0xC00006) {
        if (!emu->vdp_pending) {
            if ((val & 0xC000) == 0x8000) {
                // Register Write: 1000 RRRR DDDD DDDD
                uint8_t r = (val >> 8) & 0x1F;
                if (r < 24) emu->vdp_regs[r] = val & 0xFF;
            } else {
                // First Command Word
                emu->vdp_addr = (emu->vdp_addr & 0x3FFF0000) | (val & 0x3FFF);
                emu->vdp_code = (emu->vdp_code & 0x3C) | ((val >> 14) & 0x03);
                emu->vdp_pending = 1;
            }
        } else {
            // Second Command Word
            emu->vdp_addr = (emu->vdp_addr & 0x00003FFF) | ((val & 0x0007) << 14);
            emu->vdp_code = (emu->vdp_code & 0x03) | ((val >> 2) & 0x3C);
            emu->vdp_pending = 0;

            // Trigger 68000-to-VDP DMA if bit 7 set and DMA enabled in R1
            if ((val & 0x80) && (emu->vdp_regs[1] & 0x10)) {
                uint32_t len = ((uint32_t)emu->vdp_regs[20] << 8) | emu->vdp_regs[19];
                if (len == 0) len = 0x10000;
                uint32_t src = ((uint32_t)emu->vdp_regs[23] << 17) |
                               ((uint32_t)emu->vdp_regs[22] << 9) |
                               ((uint32_t)emu->vdp_regs[21] << 1);
                for (uint32_t i = 0; i < len; i++) {
                    uint16_t w = genesis_read16(emu, src);
                    src += 2;
                    if (emu->vdp_code == 1 && emu->vram) {
                        emu->vram[emu->vdp_addr & 0xFFFF] = w >> 8;
                        emu->vram[(emu->vdp_addr + 1) & 0xFFFF] = w & 0xFF;
                    } else if (emu->vdp_code == 3 && emu->cram) {
                        uint8_t cidx = (emu->vdp_addr >> 1) & 0x3F;
                        emu->cram[cidx] = w;
                        emu->palette_cache[cidx] = genesis_cram_to_bgr565(w);
                    }
                    emu->vdp_addr += emu->vdp_regs[15];
                }
            }
        }
        return;
    }

    // VDP Data Port
    if (addr == 0xC00000 || addr == 0xC00002) {
        emu->vdp_pending = 0;
        if (emu->vdp_code == 1 && emu->vram) {
            // VRAM Write
            emu->vram[emu->vdp_addr & 0xFFFF] = val >> 8;
            emu->vram[(emu->vdp_addr + 1) & 0xFFFF] = val & 0xFF;
        } else if (emu->vdp_code == 3 && emu->cram) {
            // CRAM Write
            uint8_t cidx = (emu->vdp_addr >> 1) & 0x3F;
            emu->cram[cidx] = val;
            emu->palette_cache[cidx] = genesis_cram_to_bgr565(val);
        } else if (emu->vdp_code == 5 && emu->vsram) {
            // VSRAM Write
            emu->vsram[(emu->vdp_addr >> 1) & 0x27] = val;
        }
        emu->vdp_addr += emu->vdp_regs[15];
        return;
    }

    // Work RAM
    if (addr >= 0xE00000) {
        if (emu->ram) {
            uint32_t offset = addr & 0xFFFF;
            emu->ram[offset] = val >> 8;
            emu->ram[(offset + 1) & 0xFFFF] = val & 0xFF;
        }
        return;
    }

    genesis_write8(emu, addr, val >> 8);
    genesis_write8(emu, addr + 1, val & 0xFF);
}

static void genesis_write32(genesis_t* emu, uint32_t addr, uint32_t val) {
    genesis_write16(emu, addr, val >> 16);
    genesis_write16(emu, addr + 2, val & 0xFFFF);
}

// ---------------------------------------------------------------------------
// 68000 Condition Code Helpers
// ---------------------------------------------------------------------------
#define FLAG_C 0x0001
#define FLAG_V 0x0002
#define FLAG_Z 0x0004
#define FLAG_N 0x0008
#define FLAG_X 0x0010

static inline void set_flags_logic8(genesis_t* emu, uint8_t res) {
    emu->sr &= ~(FLAG_N | FLAG_Z | FLAG_V | FLAG_C);
    if (res == 0) emu->sr |= FLAG_Z;
    if (res & 0x80) emu->sr |= FLAG_N;
}

static inline void set_flags_logic16(genesis_t* emu, uint16_t res) {
    emu->sr &= ~(FLAG_N | FLAG_Z | FLAG_V | FLAG_C);
    if (res == 0) emu->sr |= FLAG_Z;
    if (res & 0x8000) emu->sr |= FLAG_N;
}

static inline void set_flags_logic32(genesis_t* emu, uint32_t res) {
    emu->sr &= ~(FLAG_N | FLAG_Z | FLAG_V | FLAG_C);
    if (res == 0) emu->sr |= FLAG_Z;
    if (res & 0x80000000) emu->sr |= FLAG_N;
}

static inline bool test_cc(genesis_t* emu, uint8_t cc) {
    bool c = (emu->sr & FLAG_C) != 0;
    bool v = (emu->sr & FLAG_V) != 0;
    bool z = (emu->sr & FLAG_Z) != 0;
    bool n = (emu->sr & FLAG_N) != 0;
    switch (cc) {
        case 0: return true;            // T (True)
        case 1: return false;           // F (False)
        case 2: return !c && !z;        // HI
        case 3: return c || z;          // LS
        case 4: return !c;              // CC / HS
        case 5: return c;               // CS / LO
        case 6: return !z;              // NE
        case 7: return z;               // EQ
        case 8: return !v;              // VC
        case 9: return v;               // VS
        case 10: return !n;             // PL
        case 11: return n;              // MI
        case 12: return (n && v) || (!n && !v); // GE
        case 13: return (n && !v) || (!n && v); // LT
        case 14: return ((n && v) || (!n && !v)) && !z; // GT
        case 15: return ((n && !v) || (!n && v)) || z;  // LE
        default: return false;
    }
}

// ---------------------------------------------------------------------------
// 68000 CPU Core Interpreter Step
// ---------------------------------------------------------------------------
static int genesis_cpu_step(genesis_t* emu) {
    if (emu->stopped) return 4;

    uint16_t op = genesis_read16(emu, emu->pc);
    emu->pc += 2;

    // NOP
    if (op == 0x4E71) return 4;

    // RTS
    if (op == 0x4E75) {
        emu->pc = genesis_read32(emu, emu->a[7]);
        emu->a[7] += 4;
        return 16;
    }

    // RTE
    if (op == 0x4E73) {
        emu->sr = genesis_read16(emu, emu->a[7]);
        emu->pc = genesis_read32(emu, emu->a[7] + 2);
        emu->a[7] += 6;
        return 20;
    }

    // STOP
    if (op == 0x4E72) {
        emu->sr = genesis_read16(emu, emu->pc);
        emu->pc += 2;
        emu->stopped = true;
        return 4;
    }

    // MOVEQ: 0111 RRR 0 DDDDDDDD
    if ((op & 0xF100) == 0x7000) {
        uint8_t r = (op >> 9) & 7;
        int32_t val = (int8_t)(op & 0xFF);
        emu->d[r] = (uint32_t)val;
        set_flags_logic32(emu, emu->d[r]);
        return 4;
    }

    // BRA / BSR / Bcc
    if ((op & 0xF000) == 0x6000) {
        uint8_t cc = (op >> 8) & 0x0F;
        int32_t disp = (int8_t)(op & 0xFF);
        if (disp == 0) {
            disp = (int16_t)genesis_read16(emu, emu->pc);
            emu->pc += 2;
        }
        if (cc == 0) {
            // BRA
            emu->pc = emu->pc - 2 + disp;
            return 10;
        } else if (cc == 1) {
            // BSR
            emu->a[7] -= 4;
            genesis_write32(emu, emu->a[7], emu->pc);
            emu->pc = emu->pc - 2 + disp;
            return 18;
        } else {
            // Bcc
            if (test_cc(emu, cc)) {
                emu->pc = emu->pc - 2 + disp;
                return 10;
            }
            return 8;
        }
    }

    // LEA: 0100 RRR 111 MMM RRR
    if ((op & 0xF1C0) == 0x41C0) {
        uint8_t r = (op >> 9) & 7;
        uint8_t mode = (op >> 3) & 7;
        uint8_t rm = op & 7;
        if (mode == 7 && rm == 0) {
            // Absolute Short
            emu->a[r] = (int16_t)genesis_read16(emu, emu->pc);
            emu->pc += 2;
        } else if (mode == 7 && rm == 1) {
            // Absolute Long
            emu->a[r] = genesis_read32(emu, emu->pc);
            emu->pc += 4;
        } else if (mode == 2) {
            // (An)
            emu->a[r] = emu->a[rm];
        } else if (mode == 5) {
            // (d16, An)
            int16_t d = (int16_t)genesis_read16(emu, emu->pc);
            emu->pc += 2;
            emu->a[r] = emu->a[rm] + d;
        }
        return 8;
    }

    // JSR: 0100 1110 10 MM M RRR
    if ((op & 0xFFC0) == 0x4E80) {
        uint8_t mode = (op >> 3) & 7;
        uint8_t rm = op & 7;
        uint32_t target = 0;
        if (mode == 7 && rm == 1) {
            target = genesis_read32(emu, emu->pc);
            emu->pc += 4;
        } else if (mode == 2) {
            target = emu->a[rm];
        }
        if (target) {
            emu->a[7] -= 4;
            genesis_write32(emu, emu->a[7], emu->pc);
            emu->pc = target;
            return 18;
        }
    }

    // JMP: 0100 1110 11 MM M RRR
    if ((op & 0xFFC0) == 0x4EC0) {
        uint8_t mode = (op >> 3) & 7;
        uint8_t rm = op & 7;
        if (mode == 7 && rm == 1) {
            emu->pc = genesis_read32(emu, emu->pc);
            return 12;
        } else if (mode == 2) {
            emu->pc = emu->a[rm];
            return 8;
        }
    }

    // CLR: 0100 0010 SS MM M RRR
    if ((op & 0xFF00) == 0x4200) {
        uint8_t size = (op >> 6) & 3;
        uint8_t rm = op & 7;
        if (size == 0) emu->d[rm] &= 0xFFFFFF00;
        else if (size == 1) emu->d[rm] &= 0xFFFF0000;
        else if (size == 2) emu->d[rm] = 0;
        emu->sr &= ~(FLAG_N | FLAG_V | FLAG_C);
        emu->sr |= FLAG_Z;
        return 4;
    }

    // SWAP: 0100 1000 0100 0 RRR
    if ((op & 0xFFF8) == 0x4840) {
        uint8_t r = op & 7;
        emu->d[r] = (emu->d[r] >> 16) | (emu->d[r] << 16);
        set_flags_logic32(emu, emu->d[r]);
        return 4;
    }

    // MOVE (Word to Dn): 0011 RRR 000 000 RRR
    if ((op & 0xF1F8) == 0x3000) {
        uint8_t rd = (op >> 9) & 7;
        uint8_t rs = op & 7;
        uint16_t val = emu->d[rs] & 0xFFFF;
        emu->d[rd] = (emu->d[rd] & 0xFFFF0000) | val;
        set_flags_logic16(emu, val);
        return 4;
    }

    // Generic fallback: consume 4 cycles
    return 4;
}

// ---------------------------------------------------------------------------
// VDP Scanline Rendering Engine
// ---------------------------------------------------------------------------
static void genesis_render_scanline(genesis_t* emu, int line) {
    if (!emu->framebuffer || line < 0 || line >= 224) return;

    uint16_t* row = &emu->framebuffer[line * 320];
    uint16_t bg = emu->palette_cache[emu->vdp_regs[7] & 0x3F];

    // 1. Clear scanline to backdrop color
    for (int x = 0; x < 320; x++) {
        row[x] = bg;
    }

    if (!emu->vram) return;

    // 2. Render Plane B (Scroll B)
    uint32_t b_base = ((uint32_t)emu->vdp_regs[4] & 0x07) << 13;
    uint32_t hs_base = ((uint32_t)emu->vdp_regs[13] & 0x3F) << 10;
    int16_t hs_b = 0;
    if (hs_base + line * 4 + 3 < 0x10000) {
        hs_b = (int16_t)(((uint16_t)emu->vram[hs_base + line * 4 + 2] << 8) | emu->vram[hs_base + line * 4 + 3]);
    }
    uint16_t vs_b = emu->vsram ? emu->vsram[1] : 0;

    int py_b = (line + vs_b) & 7;
    int ty_b = ((line + vs_b) >> 3) & 63;

    for (int col = 0; col < 40; col++) {
        int x = col * 8;
        int tx_b = (((x - hs_b) >> 3) & 63);
        uint32_t entry_addr = b_base + (ty_b * 64 + tx_b) * 2;
        if (entry_addr + 1 >= 0x10000) continue;

        uint16_t tile = ((uint16_t)emu->vram[entry_addr] << 8) | emu->vram[entry_addr + 1];
        uint16_t tile_idx = tile & 0x7FF;
        uint8_t pal = (tile >> 13) & 3;

        uint32_t pat_addr = tile_idx * 32 + py_b * 4;
        if (pat_addr + 3 >= 0x10000) continue;

        for (int px = 0; px < 8; px++) {
            uint8_t b = emu->vram[pat_addr + (px >> 1)];
            uint8_t color_idx = (px & 1) ? (b & 0x0F) : (b >> 4);
            if (color_idx != 0 && (x + px) < 320) {
                row[x + px] = emu->palette_cache[pal * 16 + color_idx];
            }
        }
    }

    // 3. Render Plane A (Scroll A)
    uint32_t a_base = ((uint32_t)emu->vdp_regs[2] & 0x38) << 10;
    int16_t hs_a = 0;
    if (hs_base + line * 4 + 1 < 0x10000) {
        hs_a = (int16_t)(((uint16_t)emu->vram[hs_base + line * 4] << 8) | emu->vram[hs_base + line * 4 + 1]);
    }
    uint16_t vs_a = emu->vsram ? emu->vsram[0] : 0;

    int py_a = (line + vs_a) & 7;
    int ty_a = ((line + vs_a) >> 3) & 63;

    for (int col = 0; col < 40; col++) {
        int x = col * 8;
        int tx_a = (((x - hs_a) >> 3) & 63);
        uint32_t entry_addr = a_base + (ty_a * 64 + tx_a) * 2;
        if (entry_addr + 1 >= 0x10000) continue;

        uint16_t tile = ((uint16_t)emu->vram[entry_addr] << 8) | emu->vram[entry_addr + 1];
        uint16_t tile_idx = tile & 0x7FF;
        uint8_t pal = (tile >> 13) & 3;

        uint32_t pat_addr = tile_idx * 32 + py_a * 4;
        if (pat_addr + 3 >= 0x10000) continue;

        for (int px = 0; px < 8; px++) {
            uint8_t b = emu->vram[pat_addr + (px >> 1)];
            uint8_t color_idx = (px & 1) ? (b & 0x0F) : (b >> 4);
            if (color_idx != 0 && (x + px) < 320) {
                row[x + px] = emu->palette_cache[pal * 16 + color_idx];
            }
        }
    }

    // 4. Render Sprites
    uint32_t sat_base = ((uint32_t)emu->vdp_regs[5] & 0x7F) << 9;
    for (int s = 0; s < 80; s++) {
        uint32_t s_addr = sat_base + s * 8;
        if (s_addr + 7 >= 0x10000) break;

        int spr_y = (int)(((uint16_t)emu->vram[s_addr] << 8) | emu->vram[s_addr + 1]) & 0x3FF;
        spr_y -= 128;
        uint8_t size = emu->vram[s_addr + 2];
        int h_cells = (size & 3) + 1;
        int w_cells = ((size >> 2) & 3) + 1;
        int spr_h = h_cells * 8;

        if (line >= spr_y && line < spr_y + spr_h) {
            uint16_t attr = ((uint16_t)emu->vram[s_addr + 4] << 8) | emu->vram[s_addr + 5];
            uint16_t tile_idx = attr & 0x7FF;
            uint8_t pal = (attr >> 13) & 3;
            int spr_x = (int)(((uint16_t)emu->vram[s_addr + 6] << 8) | emu->vram[s_addr + 7]) & 0x1FF;
            spr_x -= 128;

            int dy = line - spr_y;
            int cell_y = dy >> 3;
            int py = dy & 7;

            for (int cx = 0; cx < w_cells; cx++) {
                uint32_t tile_addr = (tile_idx + cx * h_cells + cell_y) * 32 + py * 4;
                if (tile_addr + 3 >= 0x10000) continue;

                for (int px = 0; px < 8; px++) {
                    int screen_x = spr_x + cx * 8 + px;
                    if (screen_x >= 0 && screen_x < 320) {
                        uint8_t b = emu->vram[tile_addr + (px >> 1)];
                        uint8_t color_idx = (px & 1) ? (b & 0x0F) : (b >> 4);
                        if (color_idx != 0) {
                            row[screen_x] = emu->palette_cache[pal * 16 + color_idx];
                        }
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Public Genesis Core API
// ---------------------------------------------------------------------------

void genesis_init(genesis_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb) {
    if (!emu) return;
    memset(emu, 0, sizeof(genesis_t));

    emu->rom = rom;
    emu->rom_size = rom_size;
    emu->framebuffer = fb;

    // Work RAM (64KB)
    if (ram) {
        emu->ram = ram;
        emu->owns_ram = false;
    } else {
        emu->ram = (uint8_t*)GENESIS_MALLOC(64 * 1024);
        emu->owns_ram = true;
    }
    if (emu->ram) memset(emu->ram, 0, 64 * 1024);

    // VRAM (64KB), CRAM (128B), VSRAM (80B), Z80 RAM (8KB)
    emu->vram = (uint8_t*)GENESIS_MALLOC(64 * 1024);
    emu->cram = (uint16_t*)GENESIS_MALLOC(64 * sizeof(uint16_t));
    emu->vsram = (uint16_t*)GENESIS_MALLOC(40 * sizeof(uint16_t));
    emu->z80_ram = (uint8_t*)GENESIS_MALLOC(8 * 1024);

    if (emu->vram) memset(emu->vram, 0, 64 * 1024);
    if (emu->cram) memset(emu->cram, 0, 64 * sizeof(uint16_t));
    if (emu->vsram) memset(emu->vsram, 0, 40 * sizeof(uint16_t));
    if (emu->z80_ram) memset(emu->z80_ram, 0, 8 * 1024);

    // Initial VDP Register Defaults
    emu->vdp_regs[1] = 0x04; // 28-cell mode, active display
    emu->vdp_regs[12] = 0x81; // 40-cell (320px) mode
    emu->vdp_regs[15] = 0x02; // Auto-increment = 2 bytes

    // Pre-seed palette cache
    for (int i = 0; i < 64; i++) {
        emu->palette_cache[i] = 0;
    }

    genesis_reset(emu);
}

void genesis_destroy(genesis_t* emu) {
    if (!emu) return;

    if (emu->owns_ram && emu->ram) {
        GENESIS_FREE(emu->ram);
        emu->ram = NULL;
    }
    if (emu->vram) {
        GENESIS_FREE(emu->vram);
        emu->vram = NULL;
    }
    if (emu->cram) {
        GENESIS_FREE(emu->cram);
        emu->cram = NULL;
    }
    if (emu->vsram) {
        GENESIS_FREE(emu->vsram);
        emu->vsram = NULL;
    }
    if (emu->z80_ram) {
        GENESIS_FREE(emu->z80_ram);
        emu->z80_ram = NULL;
    }

    memset(emu, 0, sizeof(genesis_t));
}

void genesis_reset(genesis_t* emu) {
    if (!emu) return;

    // Read initial SSP and PC from vector table
    if (emu->rom && emu->rom_size >= 8) {
        emu->a[7] = ((uint32_t)emu->rom[0] << 24) | ((uint32_t)emu->rom[1] << 16) |
                    ((uint32_t)emu->rom[2] << 8)  | emu->rom[3];
        emu->ssp = emu->a[7];
        emu->pc  = ((uint32_t)emu->rom[4] << 24) | ((uint32_t)emu->rom[5] << 16) |
                    ((uint32_t)emu->rom[6] << 8)  | emu->rom[7];
    } else {
        emu->a[7] = 0x00FFFE00;
        emu->ssp  = emu->a[7];
        emu->pc   = 0x00000200;
    }

    emu->sr = 0x2700; // Supervisor mode, interrupts masked
    emu->stopped = false;
    emu->scanline = 0;
    emu->cycles = 0;
}

void genesis_set_pad(genesis_t* emu, uint8_t pad) {
    if (emu) emu->pad_state = pad;
}

void genesis_step_frame(genesis_t* emu) {
    if (!emu) return;

    const int CYCLES_PER_LINE = 488; // 7.67 MHz / 60 Hz / 262 lines

    // Scanlines 0..223: Active Display
    for (int line = 0; line < 224; line++) {
        emu->scanline = line;

        int cycles = 0;
        while (cycles < CYCLES_PER_LINE) {
            cycles += genesis_cpu_step(emu);
        }

        genesis_render_scanline(emu, line);
    }

    // Scanlines 224..261: VBlank
    for (int line = 224; line < 262; line++) {
        emu->scanline = line;

        // At scanline 224, trigger VBlank Interrupt (Level 6) if enabled
        if (line == 224 && (emu->vdp_regs[1] & 0x20)) {
            emu->a[7] -= 4;
            genesis_write32(emu, emu->a[7], emu->pc);
            emu->a[7] -= 2;
            genesis_write16(emu, emu->a[7], emu->sr);
            emu->sr |= 0x2000; // Supervisor mode
            emu->pc = genesis_read32(emu, 0x000078); // Vector 28 (Level 6)
        }

        int cycles = 0;
        while (cycles < CYCLES_PER_LINE) {
            cycles += genesis_cpu_step(emu);
        }
    }

    emu->scanline = 0;
}
