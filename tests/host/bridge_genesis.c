/* Host execution harness for Gwenesis (Sega Genesis / Mega Drive 68K). */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bus/gwenesis_bus.h"
#include "cpus/M68K/m68k.h"
#include "io/gwenesis_io.h"
#include "vdp/gwenesis_vdp.h"
#include "sound/ym2612.h"
#include "sound/gwenesis_sn76489.h"
#include "sound/z80inst.h"
#include "savestate/gwenesis_savestate.h"

int scan_line = 0;
int system_clock = 0;
int frame_counter = 0;
uint16_t v_counter = 0;
int16_t* gwenesis_ym2612_buffer = NULL;
int16_t* gwenesis_sn76489_buffer = NULL;

static uint16_t* s_gen_fb = NULL;

void genesis_save_mark_dirty_c(void) {}

void GWENESIS_PUSH_SCANLINE(int line, const uint16_t* src16, int w) {
  if (!s_gen_fb || !src16 || line < 0 || line >= 224 || w <= 0) return;
  uint16_t* dst = s_gen_fb + (size_t)line * 320U;
  int copy_w = (w >= 320) ? 320 : w;
  memcpy(dst, src16, (size_t)copy_w * sizeof(uint16_t));
}

void gwenesis_io_get_buttons(void) {
  for (int b = 0; b < 8; ++b) {
    gwenesis_io_pad_release_button(0, b);
  }
}

int test_genesis_core(void) {
  s_gen_fb = (uint16_t*)calloc(320U * 224U, sizeof(uint16_t));
  uint8_t* rom = (uint8_t*)calloc(1U, 1024U);
  VRAM = (unsigned char*)calloc(1U, VRAM_MAX_SIZE);
  M68K_RAM = (unsigned char*)calloc(1U, MAX_RAM_SIZE);
  ZRAM = (unsigned char*)calloc(1U, MAX_Z80_RAM_SIZE);
  if (!s_gen_fb || !rom || !VRAM || !M68K_RAM || !ZRAM) {
    free(s_gen_fb); free(rom); free(VRAM); free(M68K_RAM); free(ZRAM);
    s_gen_fb = NULL; VRAM = NULL; M68K_RAM = NULL; ZRAM = NULL;
    printf("[HOST_CORE] GENESIS_68K STATUS=FAIL (oom)\n");
    return 1;
  }

  /* M68K vectors: SSP=$00FF8000, PC=$00000200 */
  rom[0] = 0x00U; rom[1] = 0xFFU; rom[2] = 0x80U; rom[3] = 0x00U;
  rom[4] = 0x00U; rom[5] = 0x00U; rom[6] = 0x02U; rom[7] = 0x00U;
  memcpy(&rom[0x100], "SEGA MEGA DRIVE ", 16);
  memcpy(&rom[0x1F0], "U               ", 16);

  /* Big-endian M68K machine code at $0200:
   * MOVE.L #$1234564D, D0
   * MOVE.W #$0042, $00FF0000
   * MOVE.L #$80048144, $00C00004  (VDP Reg0=04, Reg1=44 display enable)
   * MOVE.L #$87000000, $00C00004  (VDP Reg7=00)
   * MOVE.L #$C0000000, $00C00004  (CRAM write addr 0)
   * MOVE.W #$00EE, $00C00000      (CRAM[0] = 0x0EE yellow)
   * BRA.S -2
   */
  const uint8_t prog[] = {
    0x20U, 0x3CU, 0x12U, 0x34U, 0x56U, 0x4DU,
    0x33U, 0xFCU, 0x00U, 0x42U, 0x00U, 0xFFU, 0x00U, 0x00U,
    0x23U, 0xFCU, 0x80U, 0x04U, 0x81U, 0x44U, 0x00U, 0xC0U, 0x00U, 0x04U,
    0x23U, 0xFCU, 0xC0U, 0x00U, 0x00U, 0x00U, 0x00U, 0xC0U, 0x00U, 0x04U,
    0x33U, 0xFCU, 0x00U, 0xEEU, 0x00U, 0xC0U, 0x00U, 0x00U,
    0x60U, 0xFEU
  };
  memcpy(&rom[0x200], prog, sizeof(prog));

  gwenesis_vdp_allocate_buffers();
  gwenesis_ym2612_tables_alloc();
  load_cartridge(rom, 1024U);
  gwenesis_init_sram(rom, 1024U);
  power_on();
  reset_emulation();

  scan_line = 0;
  system_clock = 0;
  frame_counter = 0;
  v_counter = 0;

  for (int f = 0; f < 50; ++f) {
    gwenesis_vdp_render_config();
    int visible_h = REG1_PAL ? 240 : 224;
    int lines = REG1_PAL ? 313 : 262;
    int deadline = 0;
    for (scan_line = 0; scan_line < lines; ++scan_line) {
      deadline += VDP_CYCLES_PER_LINE;
      system_clock = deadline;
      v_counter = (uint16_t)scan_line;
      m68k_run((unsigned int)deadline);
      z80_run(deadline);
      if (scan_line < visible_h && scan_line < 224) {
        gwenesis_vdp_render_line(scan_line);
      }
    }
    m68k.cycles -= deadline;
    ++frame_counter;
  }

  unsigned int d0 = m68k_get_reg(M68K_REG_D0);
  uint16_t ram0 = (uint16_t)m68k_read_memory_16(0x00FF0000U);
  int nz = 0;
  for (size_t i = 0; i < 320U * 224U; ++i) {
    if (s_gen_fb[i] != 0U) ++nz;
  }

  saveGwenesisStateResetArchive();
  gwenesis_vdp_free_buffers();
  gwenesis_ym2612_free();
  m68k_free();
  gwenesis_bus_free();
  free(s_gen_fb);
  free(rom);
  s_gen_fb = NULL;

  if (d0 != 0x1234564DU || ram0 != 0x0042U || nz < 1000) {
    printf("[HOST_CORE] GENESIS_68K STATUS=FAIL d0=0x%08X ram0=0x%04X non_zero=%d\n",
           d0, ram0, nz);
    return 1;
  }
  printf("[HOST_CORE] GENESIS_68K STATUS=PASS d0=0x%08X ram0=0x%04X non_zero=%d\n",
         d0, ram0, nz);
  return 0;
}
