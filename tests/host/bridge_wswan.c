/* Host execution harness for Oswan (Bandai WonderSwan / WonderSwan Color). */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WS.h"
#include "WSRender.h"
#include "WSFileio.h"

static uint16_t* s_ws_panel = NULL;

void ws_graphics_paint(void) {
  if (!s_ws_panel || !FrameBuffer) return;
  for (int y = 0; y < 144; ++y) {
    const uint16_t* src = FrameBuffer + (size_t)y * SCREEN_WIDTH;
    uint16_t* dst = s_ws_panel + (size_t)y * 224U;
    memcpy(dst, src, 224U * sizeof(uint16_t));
  }
}

int ws_input_poll(int mode) {
  (void)mode;
  return 0;
}

unsigned long SDL_UXTimerRead(void) {
  static unsigned long g_ws_timer = 0;
  g_ws_timer += 200000UL;
  return g_ws_timer;
}

int test_wswan_core(void) {
  s_ws_panel = (uint16_t*)calloc(224U * 144U, sizeof(uint16_t));
  uint8_t* rom = (uint8_t*)calloc(1U, 0x10000U);
  if (!s_ws_panel || !rom) {
    free(s_ws_panel);
    free(rom);
    s_ws_panel = NULL;
    printf("[HOST_CORE] WONDERSWAN STATUS=FAIL (oom)\n");
    return 1;
  }

  /* 8086/V30MZ program at offset 0x0000 (F000:0000):
   * MOV AL,0x5C ; MOV [0x0010],AL
   * Turn on LCD (IO[0x14]=1), color mode (IO[0x60]=0xE0),
   * write white RGB444 into palette RAM 0xFE00 = 0x0FFF, set BORDER=0 (IO[0x01]=0), loop. */
  const uint8_t prog[] = {
    0xB0U, 0x5CU,               /* MOV AL, 0x5C */
    0xA2U, 0x10U, 0x00U,        /* MOV [0x0010], AL (IRAM[0x10]) */
    0xB8U, 0xFFU, 0x0FU,        /* MOV AX, 0x0FFF */
    0xA3U, 0x00U, 0xFEU,        /* MOV [0xFE00], AX (Palette[0][0]) */
    0xB0U, 0xE0U, 0xE6U, 0x60U, /* MOV AL, 0xE0 ; OUT 0x60, AL (16-color/4-color mode) */
    0xB0U, 0x00U, 0xE6U, 0x01U, /* MOV AL, 0x00 ; OUT 0x01, AL (BORDER = pal0 col0) */
    0xB0U, 0x01U, 0xE6U, 0x14U, /* MOV AL, 0x01 ; OUT 0x14, AL (LCDSLP LCD ON) */
    0xEBU, 0xFEU                /* JMP $ */
  };
  memcpy(rom, prog, sizeof(prog));

  /* Reset vector at FFFF:0000 (ROM offset 0xFFF0): JMP FAR F000:0000 */
  rom[0xFFF0] = 0xEAU;
  rom[0xFFF1] = 0x00U;
  rom[0xFFF2] = 0x00U;
  rom[0xFFF3] = 0x00U;
  rom[0xFFF4] = 0xF0U;

  /* 10-byte WonderSwan trailer at 0xFFF6..0xFFFF */
  rom[0xFFF6] = 0x01U; /* developer ID */
  rom[0xFFF7] = 0x01U; /* WSC capable */
  rom[0xFFF8] = 0x00U;
  rom[0xFFFA] = 0x02U; /* 1Mbit indicator minimal */
  rom[0xFFFB] = 0x00U; /* no SRAM */

  WsInit();
  if (WsCreateFromMemory(rom, 0x10000) != 0) {
    WsDeInit();
    free(s_ws_panel);
    free(rom);
    s_ws_panel = NULL;
    printf("[HOST_CORE] WONDERSWAN STATUS=FAIL (init)\n");
    return 1;
  }
  WsReset();

  for (int f = 0; f < 50; ++f) {
    WsRun();
  }

  uint8_t iram10 = IRAM ? IRAM[0x10] : 0U;
  int nz = 0;
  for (size_t i = 0; i < 224U * 144U; ++i) {
    if (s_ws_panel[i] != 0U) ++nz;
  }

  WsDeInit();
  free(s_ws_panel);
  free(rom);
  s_ws_panel = NULL;

  if (iram10 != 0x5CU || nz < 1000) {
    printf("[HOST_CORE] WONDERSWAN STATUS=FAIL iram10=0x%02X non_zero=%d\n", iram10, nz);
    return 1;
  }
  printf("[HOST_CORE] WONDERSWAN STATUS=PASS iram10=0x%02X non_zero=%d\n", iram10, nz);
  return 0;
}
