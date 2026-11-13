/* Host execution harness for huexpress (PC Engine / TurboGrafx-16). */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pce-go.h"
#include "pce.h"

static uint8_t* s_raw_fb = NULL;
static int s_vsync_count = 0;
static int s_nonzero_pixels = 0;

uint8_t* osd_gfx_framebuffer(int width, int height) {
  (void)width;
  (void)height;
  return s_raw_fb ? (s_raw_fb + 16) : NULL;
}

void osd_input_read(uint8_t joypads[8]) {
  memset(joypads, 0, 8U);
}

void osd_vsync(void) {
  ++s_vsync_count;
  if (!s_raw_fb) return;
  uint32_t w = PCE.VDC.screen_width ? PCE.VDC.screen_width : 256U;
  uint32_t h = PCE.VDC.screen_height ? PCE.VDC.screen_height : 224U;
  if (w > 352U) w = 256U;
  if (h > (uint32_t)XBUF_HEIGHT) h = 224U;
  const uint8_t* px = s_raw_fb + 16;
  int nz = 0;
  for (uint32_t y = 0; y < h; ++y) {
    const uint8_t* row = px + y * (uint32_t)XBUF_WIDTH;
    for (uint32_t x = 0; x < w; ++x) {
      if (row[x] != 0U) ++nz;
    }
  }
  s_nonzero_pixels = nz;
}

int test_pce_core(void) {
  s_raw_fb = (uint8_t*)calloc(1U, 16U + (size_t)XBUF_WIDTH * (size_t)(XBUF_HEIGHT + 1));
  uint8_t* rom = (uint8_t*)calloc(1U, 8192U);
  if (!s_raw_fb || !rom) {
    free(s_raw_fb);
    free(rom);
    s_raw_fb = NULL;
    printf("[HOST_CORE] PC_ENGINE STATUS=FAIL (oom)\n");
    return 1;
  }

  /* Reset vector at $FFFE -> $E000 */
  rom[0x1FFE] = 0x00U;
  rom[0x1FFF] = 0xE0U;

  /* Map RAM ($F8) into MPR1 ($2000), hardware ($FF) into MPR0 ($0000),
   * write RAM marker $2000 = 0x42, set VCE[0] = 0x01C7, enable VDC display, loop. */
  const uint8_t prog[] = {
      0xA9U, 0xF8U, 0x53U, 0x02U,        /* LDA #$F8 ; TAM #$02 */
      0xA9U, 0xFFU, 0x53U, 0x01U,        /* LDA #$FF ; TAM #$01 */
      0xA9U, 0x42U, 0x8DU, 0x00U, 0x20U, /* LDA #$42 ; STA $2000 */
      0x9CU, 0x02U, 0x04U,               /* STZ $0402 */
      0x9CU, 0x03U, 0x04U,               /* STZ $0403 */
      0xA9U, 0xC7U, 0x8DU, 0x04U, 0x04U, /* LDA #$C7 ; STA $0404 */
      0xA9U, 0x01U, 0x8DU, 0x05U, 0x04U, /* LDA #$01 ; STA $0405 */
      0xA9U, 0x05U, 0x8DU, 0x00U, 0x00U, /* LDA #$05 ; STA $0000 */
      0xA9U, 0xC0U, 0x8DU, 0x02U, 0x00U, /* LDA #$C0 ; STA $0002 */
      0x9CU, 0x03U, 0x00U,               /* STZ $0003 */
      0x80U, 0xFEU                       /* BRA -2 */
  };
  memcpy(rom, prog, sizeof(prog));

  s_vsync_count = 0;
  s_nonzero_pixels = 0;

  if (InitPCE(22050, false) != 0 || LoadCardBorrowed(rom, 8192U) != 0) {
    ShutdownPCE();
    free(s_raw_fb);
    free(rom);
    s_raw_fb = NULL;
    printf("[HOST_CORE] PC_ENGINE STATUS=FAIL (init)\n");
    return 1;
  }

  ResetPCE(false);
  for (int f = 0; f < 50; ++f) {
    StepPCE();
  }

  uint8_t ram0 = PCE.RAM ? PCE.RAM[0] : 0U;
  uint16_t vce0 = (PCE.VCE.regs) ? PCE.VCE.regs[0].W : 0U;

  ShutdownPCE();
  free(s_raw_fb);
  free(rom);
  s_raw_fb = NULL;

  if (ram0 != 0x42U || vce0 != 0x01C7U || s_vsync_count != 50) {
    printf("[HOST_CORE] PC_ENGINE STATUS=FAIL ram0=0x%02X vce0=0x%04X vsync=%d px=%d\n", ram0, vce0, s_vsync_count,
           s_nonzero_pixels);
    return 1;
  }
  printf("[HOST_CORE] PC_ENGINE STATUS=PASS ram0=0x%02X vce0=0x%04X vsync=%d\n", ram0, vce0, s_vsync_count);
  return 0;
}
