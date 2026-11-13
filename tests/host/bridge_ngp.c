/* Host execution harness for RACE (Neo Geo Pocket / Neo Geo Pocket Color). */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "retro_compat.h"
#include "race-memory.h"
#include "cz80.h"
#include "tlcs900h.h"
#include "cz80_support.h"
#include "graphics.h"
#include "input.h"
#include "flash.h"
#include "neopopsound.h"

extern int finscan;

int m_bIsActive = 0;
EMUINFO m_emuInfo = {NGPC, 0, {0}};
struct ngp_screen* race_ngp_screen = NULL;
char retro_save_directory[3] = "/";
int tipo_consola = 0;
int gfx_hacks = 0;
volatile unsigned g_frame_ready = 0;
bool s_interlace_parity = false;
uint8_t* s_lut_y_render = NULL;
unsigned short* drawBuffer = NULL;

unsigned char* rasterY = NULL;
unsigned char* frame0Pri = NULL;
unsigned char* frame1Pri = NULL;
unsigned char* color_switch = NULL;
unsigned char* scanlineY = NULL;
unsigned char* scrollSpriteX = NULL;
unsigned char* scrollSpriteY = NULL;
unsigned char* sprite_palette_numbers = NULL;
unsigned char* sprite_table = NULL;
unsigned short* patterns = NULL;
unsigned char* oowSelect = NULL;
unsigned short* oowTable = NULL;
unsigned char* wndTopLeftY = NULL;
unsigned char* wndSizeY = NULL;
unsigned char* wndSizeX = NULL;
unsigned char* bgSelect = NULL;
unsigned char* bw_palette_table = NULL;
unsigned short* palette_table = NULL;
unsigned short* bgTable = NULL;
unsigned char* wndTopLeftX = NULL;
unsigned char* scrollFrontY = NULL;
unsigned char* scrollFrontX = NULL;
unsigned short* tile_table_front = NULL;
unsigned char* scrollBackY = NULL;
unsigned short* tile_table_back = NULL;
unsigned char* scrollBackX = NULL;
unsigned char* pattern_table = NULL;

static unsigned char s_dummy_scanline = 0;

static void map_vdp_tables(void) {
  sprite_table = (unsigned char*)get_address(0x00008800U);
  pattern_table = (unsigned char*)get_address(0x0000A000U);
  patterns = (unsigned short*)pattern_table;
  tile_table_front = (unsigned short*)get_address(0x00009000U);
  tile_table_back = (unsigned short*)get_address(0x00009800U);
  palette_table = (unsigned short*)get_address(0x00008200U);
  bw_palette_table = (unsigned char*)get_address(0x00008100U);
  sprite_palette_numbers = (unsigned char*)get_address(0x00008C00U);

  scanlineY = (unsigned char*)get_address(0x00008009U);
  frame0Pri = (unsigned char*)get_address(0x00008000U);
  frame1Pri = (unsigned char*)get_address(0x00008030U);
  wndTopLeftX = (unsigned char*)get_address(0x00008002U);
  wndTopLeftY = (unsigned char*)get_address(0x00008003U);
  wndSizeX = (unsigned char*)get_address(0x00008004U);
  wndSizeY = (unsigned char*)get_address(0x00008005U);
  scrollSpriteX = (unsigned char*)get_address(0x00008020U);
  scrollSpriteY = (unsigned char*)get_address(0x00008021U);
  scrollFrontX = (unsigned char*)get_address(0x00008032U);
  scrollFrontY = (unsigned char*)get_address(0x00008033U);
  scrollBackX = (unsigned char*)get_address(0x00008034U);
  scrollBackY = (unsigned char*)get_address(0x00008035U);
  bgSelect = (unsigned char*)get_address(0x00008118U);
  bgTable = (unsigned short*)get_address(0x000083E0U);
  oowSelect = (unsigned char*)get_address(0x00008012U);
  oowTable = (unsigned short*)get_address(0x000083F0U);
  color_switch = (unsigned char*)get_address(0x00006F91U);

  if (!scanlineY) scanlineY = &s_dummy_scanline;
  rasterY = scanlineY;
}

static void cleanup_race(uint8_t* rom, uint16_t* raw_fb) {
  m_bIsActive = 0;
  flashShutdown();
  tlcs_free_tables();
  graphics_free();
  Cz80_free_flag_tables();
  neopopsound_free();
  ngp_mem_free();
  drawBuffer = NULL;
  free(s_lut_y_render);
  s_lut_y_render = NULL;
  free(raw_fb);
  free(rom);
}

int test_ngp_core(void) {
  uint8_t* rom = (uint8_t*)calloc(1U, 65536U);
  uint16_t* raw_fb = (uint16_t*)calloc(160U * 152U, sizeof(uint16_t));
  s_lut_y_render = (uint8_t*)calloc(256U, sizeof(uint8_t));
  if (!rom || !raw_fb || !s_lut_y_render) {
    cleanup_race(rom, raw_fb);
    printf("[HOST_CORE] NGP_COLOR STATUS=FAIL (oom)\n");
    return 1;
  }
  memset(s_lut_y_render, 1, 152U);
  drawBuffer = raw_fb;

  memcpy(rom, "COPYRIGHT BY SNK CORPORATION", 28);
  /* Entry PC = 0x00200100 (LE) */
  rom[0x1C] = 0x00U;
  rom[0x1D] = 0x01U;
  rom[0x1E] = 0x20U;
  rom[0x1F] = 0x00U;
  rom[0x23] = 0x10U; /* NGPC flag */

  /* TLCS-900H at ROM+0x100: write 0x7E -> $5000, 0x0FFF -> $83E0 (bgTable[0]), 0x80 -> $8118, JR -2 */
  const uint8_t prog[] = {
      0xF1U, 0x00U, 0x50U, 0x00U, 0x7EU,        /* LD (5000h), 7Eh */
      0xF1U, 0xE0U, 0x83U, 0x02U, 0xFFU, 0x0FU, /* LDW (83E0h), 0FFFh */
      0xF1U, 0x18U, 0x81U, 0x00U, 0x80U,        /* LD (8118h), 80h */
      0x68U, 0xFEU                              /* JR T, -2 */
  };
  memcpy(rom + 0x100U, prog, sizeof(prog));

  if (!ngp_mem_set_rom(rom, 65536U)) {
    cleanup_race(rom, raw_fb);
    printf("[HOST_CORE] NGP_COLOR STATUS=FAIL (set_rom)\n");
    return 1;
  }
  setFlashSize(65536U);
  if (!ngp_mem_alloc_init(0x18000U, 0x10000U)) {
    cleanup_race(rom, raw_fb);
    printf("[HOST_CORE] NGP_COLOR STATUS=FAIL (mem_alloc)\n");
    return 1;
  }

  m_emuInfo.machine = NGPC;
  m_emuInfo.romSize = 65536;
  tipo_consola = 0;

  Cz80_allocate_flag_tables();
  ngp_mem_init();
  flashStartup();
  map_vdp_tables();

  tlcs_init();
  tlcs_reinit();
  Z80_Init();
  Z80_Reset();

  tlcsMemWriteB(0x00006F91U, tlcsMemReadB(0x00200023U));
  tlcsMemWriteB(0x00006F87U, 0x01U);
  tlcsMemWriteB(0x00004000U, tlcsMemReadB(0x00004000U) | 0xC0U);
  tlcsMemWriteB(0x00006F84U, 0x40U);

  if (!graphics_init()) {
    cleanup_race(rom, raw_fb);
    printf("[HOST_CORE] NGP_COLOR STATUS=FAIL (graphics_init)\n");
    return 1;
  }
  if (wndTopLeftX && wndTopLeftY && wndSizeX && wndSizeY) {
    *wndTopLeftX = 0;
    *wndTopLeftY = 0;
    *wndSizeX = 160;
    *wndSizeY = 152;
  }
  if (frame0Pri) *frame0Pri |= 0xC0U;
  finscan = 198;
  m_bIsActive = 1;

  for (int f = 0; f < 50; ++f) {
    tlcs_execute(6144000 / 60);
  }

  uint8_t marker = tlcsMemReadB(0x00005000U);
  int nz = 0;
  for (size_t i = 0; i < 160U * 152U; ++i) {
    if (raw_fb[i] != 0U) ++nz;
  }

  cleanup_race(rom, raw_fb);

  if (marker != 0x7EU || nz < 1000) {
    printf("[HOST_CORE] NGP_COLOR STATUS=FAIL ram5000=0x%02X non_zero=%d\n", marker, nz);
    return 1;
  }
  printf("[HOST_CORE] NGP_COLOR STATUS=PASS ram5000=0x%02X non_zero=%d\n", marker, nz);
  return 0;
}
