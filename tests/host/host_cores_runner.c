/*
 * tests/host/host_cores_runner.c - Host C Execution Harness for Genuine Emulator Cores
 *
 * Supplies minimal HAL memory/timer shims and executes 50-frame homebrew tests
 * across all 11 genuine upstream emulator cores under firmware/BmoGameboy/src/third_party/.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agnes/agnes.h"
#include "smsplus/smsplus_bmo.h"

/* External per-core C/C++ harnesses kept in separate TUs to isolate core macros. */
extern int test_atari_core_cpp(void);
extern int test_lynx_core_cpp(void);
extern int test_pico_core_cpp(void);
extern int test_ngp_core(void);
extern int test_snes_core(void);
extern int test_wswan_core(void);
extern int test_genesis_core(void);
extern int test_pce_core(void);

/* Host implementations of ESP-IDF heap_caps and Arduino millis for third_party cores. */
void* heap_caps_malloc(size_t size, uint32_t caps) {
  (void)caps;
  return malloc(size ? size : 1U);
}

void* heap_caps_calloc(size_t n, size_t size, uint32_t caps) {
  (void)caps;
  return calloc(n ? n : 1U, size ? size : 1U);
}

void* heap_caps_realloc(void* ptr, size_t size, uint32_t caps) {
  (void)caps;
  return realloc(ptr, size ? size : 1U);
}

void heap_caps_free(void* ptr) {
  free(ptr);
}

size_t heap_caps_get_free_size(uint32_t caps) {
  (void)caps;
  return 8U * 1024U * 1024U;
}

size_t heap_caps_get_largest_free_block(uint32_t caps) {
  (void)caps;
  return 4U * 1024U * 1024U;
}

unsigned long millis(void) {
  static unsigned long g_tick_ms = 0;
  g_tick_ms += 16UL;
  return g_tick_ms;
}

static int count_nonzero_and_distinct_u16(const uint16_t* buf, size_t count, int* out_distinct) {
  int nz = 0;
  int distinct = 0;
  uint16_t seen[16];
  memset(seen, 0, sizeof(seen));
  for (size_t i = 0; i < count; ++i) {
    uint16_t v = buf[i];
    if (v != 0U) {
      ++nz;
      int found = 0;
      for (int k = 0; k < distinct; ++k) {
        if (seen[k] == v) {
          found = 1;
          break;
        }
      }
      if (!found && distinct < 16) {
        seen[distinct++] = v;
      }
    }
  }
  if (out_distinct) {
    *out_distinct = distinct;
  }
  return nz;
}

static int test_colem_core(void) {
  uint8_t rom[8192];
  memset(rom, 0, sizeof(rom));
  rom[0] = 0x55U;
  rom[1] = 0xAAU;
  rom[0x0A] = 0x20U;
  rom[0x0B] = 0x80U;

  /* Enable TMS9918 display (VDP R1=0xC0), backdrop cyan/blue (VDP R7=0x5A), write $6000=$5A, loop. */
  const uint8_t prog[] = {
      0x3EU, 0x5AU, 0x32U, 0x00U, 0x60U, /* LD A,5Ah ; LD (6000h),A */
      0x3EU, 0xC0U, 0xD3U, 0xBFU,        /* LD A,C0h ; OUT (BFh),A */
      0x3EU, 0x81U, 0xD3U, 0xBFU,        /* LD A,81h ; OUT (BFh),A (VDP R1=C0h) */
      0x3EU, 0x5AU, 0xD3U, 0xBFU,        /* LD A,5Ah ; OUT (BFh),A */
      0x3EU, 0x87U, 0xD3U, 0xBFU,        /* LD A,87h ; OUT (BFh),A (VDP R7=5Ah) */
      0x18U, 0xFEU                       /* JR -2 */
  };
  memcpy(&rom[0x20], prog, sizeof(prog));

  if (!bmo_sms_init(rom, sizeof(rom), BMO_SMS_CONSOLE_COLECO, NULL)) {
    printf("[HOST_CORE] COLECO STATUS=FAIL (init)\n");
    return 1;
  }

  uint16_t* fb = (uint16_t*)calloc(256U * 192U, sizeof(uint16_t));
  if (!fb) {
    bmo_sms_destroy();
    printf("[HOST_CORE] COLECO STATUS=FAIL (oom)\n");
    return 1;
  }

  for (int f = 0; f < 50; ++f) {
    bmo_sms_run_frame(0);
  }
  bmo_sms_render_panel565(fb);

  size_t wram_sz = 0;
  const uint8_t* wram = bmo_sms_wram_data(&wram_sz);
  uint8_t marker = (wram && wram_sz > 0U) ? wram[0] : 0U;
  int distinct = 0;
  int nz = count_nonzero_and_distinct_u16(fb, 256U * 192U, &distinct);

  free(fb);
  bmo_sms_destroy();

  if (marker != 0x5AU || nz < 1000) {
    printf("[HOST_CORE] COLECO STATUS=FAIL wram0=0x%02X non_zero=%d distinct=%d\n", marker, nz, distinct);
    return 1;
  }
  printf("[HOST_CORE] COLECO STATUS=PASS wram0=0x%02X non_zero=%d distinct=%d\n", marker, nz, distinct);
  return 0;
}

static int test_sms_core(void) {
  uint8_t rom[32768];
  memset(rom, 0, sizeof(rom));

  /* Enable SMS VDP R1=0xC0, R7=0x00, CRAM[0]=0x3F (white) & CRAM[16]=0x3F, write WRAM[0]=0x7B, loop. */
  const uint8_t prog[] = {
      0x3EU, 0x7BU, 0x32U, 0x00U, 0xC0U,                      /* LD A,7Bh ; LD (C000h),A */
      0x3EU, 0xC0U, 0xD3U, 0xBFU,                             /* LD A,C0h ; OUT (BFh),A */
      0x3EU, 0x81U, 0xD3U, 0xBFU,                             /* LD A,81h ; OUT (BFh),A (VDP R1=C0h) */
      0x3EU, 0x00U, 0xD3U, 0xBFU,                             /* LD A,00h ; OUT (BFh),A */
      0x3EU, 0x87U, 0xD3U, 0xBFU,                             /* LD A,87h ; OUT (BFh),A (VDP R7=00h backdrop idx) */
      0x3EU, 0x00U, 0xD3U, 0xBFU,                             /* CRAM addr 0 */
      0x3EU, 0xC0U, 0xD3U, 0xBFU, 0x3EU, 0x3FU, 0xD3U, 0xBEU, /* CRAM[0] = 0x3F */
      0x3EU, 0x10U, 0xD3U, 0xBFU,                             /* CRAM addr 16 (sprite/backdrop bank) */
      0x3EU, 0xC0U, 0xD3U, 0xBFU, 0x3EU, 0x3FU, 0xD3U, 0xBEU, /* CRAM[16] = 0x3F */
      0x18U, 0xFEU                                            /* JR -2 */
  };
  memcpy(rom, prog, sizeof(prog));

  if (!bmo_sms_init(rom, sizeof(rom), BMO_SMS_CONSOLE_SMS, NULL)) {
    printf("[HOST_CORE] SMS_Z80 STATUS=FAIL (init)\n");
    return 1;
  }

  uint16_t* fb = (uint16_t*)calloc(256U * 192U, sizeof(uint16_t));
  if (!fb) {
    bmo_sms_destroy();
    printf("[HOST_CORE] SMS_Z80 STATUS=FAIL (oom)\n");
    return 1;
  }

  for (int f = 0; f < 50; ++f) {
    bmo_sms_run_frame(0);
  }
  bmo_sms_render_panel565(fb);

  size_t wram_sz = 0;
  const uint8_t* wram = bmo_sms_wram_data(&wram_sz);
  uint8_t marker = (wram && wram_sz > 0U) ? wram[0] : 0U;
  int distinct = 0;
  int nz = count_nonzero_and_distinct_u16(fb, 256U * 192U, &distinct);

  free(fb);
  bmo_sms_destroy();

  if (marker != 0x7BU || nz < 1000) {
    printf("[HOST_CORE] SMS_Z80 STATUS=FAIL wram0=0x%02X non_zero=%d distinct=%d\n", marker, nz, distinct);
    return 1;
  }
  printf("[HOST_CORE] SMS_Z80 STATUS=PASS wram0=0x%02X non_zero=%d distinct=%d\n", marker, nz, distinct);
  return 0;
}

static int capture_agnes_framebuffer(agnes_t* agn, uint32_t* fb) {
  int nz = 0;
  for (int y = 0; y < AGNES_SCREEN_HEIGHT; ++y) {
    for (int x = 0; x < AGNES_SCREEN_WIDTH; ++x) {
      agnes_color_t c = agnes_get_screen_pixel(agn, x, y);
      uint32_t packed = ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
      fb[(size_t)y * AGNES_SCREEN_WIDTH + (size_t)x] = packed;
      if (packed != 0U) ++nz;
    }
  }
  return nz;
}

static uint8_t* build_nes_test_ines(size_t* out_size) {
  const size_t rom_size = 16U + 16384U + 8192U;
  uint8_t* rom = (uint8_t*)calloc(1U, rom_size);
  if (!rom) return NULL;
  rom[0] = 'N';
  rom[1] = 'E';
  rom[2] = 'S';
  rom[3] = 0x1AU;
  rom[4] = 1U;
  rom[5] = 1U;
  uint8_t* prg = rom + 16U;
  memset(prg, 0xEAU, 16384U);
  prg[0x3FFCU] = 0x00U;
  prg[0x3FFDU] = 0x80U;
  const uint8_t prog[] = {0xA9U, 0x5CU, 0x8DU, 0x00U, 0x60U, 0xA9U, 0x3FU, 0x8DU, 0x06U, 0x20U, 0xA9U,
                          0x00U, 0x8DU, 0x06U, 0x20U, 0xA9U, 0x21U, 0x8DU, 0x07U, 0x20U, 0xA9U, 0x08U,
                          0x8DU, 0x01U, 0x20U, 0xE8U, 0x8EU, 0x01U, 0x60U, 0x4CU, 0x19U, 0x80U};
  memcpy(prg, prog, sizeof(prog));
  *out_size = rom_size;
  return rom;
}

static int step_agnes_frames(agnes_t* agn, int frames) {
  for (int f = 0; f < frames; ++f) {
    if (!agnes_next_frame(agn)) return 0;
  }
  return 1;
}

static int test_nes_agnes_core(void) {
  size_t rom_size = 0;
  uint8_t* rom = build_nes_test_ines(&rom_size);
  if (!rom) {
    printf("[HOST_CORE] NES_AGNES STATUS=FAIL (oom)\n");
    return 1;
  }
  agnes_t* agn = agnes_make();
  if (!agn || !agnes_load_ines_data(agn, rom, rom_size)) {
    if (agn) agnes_destroy(agn);
    free(rom);
    printf("[HOST_CORE] NES_AGNES STATUS=FAIL (init)\n");
    return 1;
  }
  agnes_input_t inp;
  memset(&inp, 0, sizeof(inp));
  agnes_set_input(agn, &inp, &inp);

  agnes_state_t* snap = (agnes_state_t*)malloc(agnes_state_size());
  uint32_t* fb_a = (uint32_t*)calloc((size_t)AGNES_SCREEN_WIDTH * AGNES_SCREEN_HEIGHT, sizeof(uint32_t));
  uint32_t* fb_b = (uint32_t*)calloc((size_t)AGNES_SCREEN_WIDTH * AGNES_SCREEN_HEIGHT, sizeof(uint32_t));
  int ok = (snap && fb_a && fb_b && step_agnes_frames(agn, 25));
  if (ok) {
    agnes_dump_state(agn, snap);
    ok = step_agnes_frames(agn, 25);
  }
  int nz = ok ? capture_agnes_framebuffer(agn, fb_a) : 0;
  ok = ok && agnes_restore_state(agn, snap) && step_agnes_frames(agn, 25);
  if (ok) capture_agnes_framebuffer(agn, fb_b);
  int match = ok && (memcmp(fb_a, fb_b, (size_t)AGNES_SCREEN_WIDTH * AGNES_SCREEN_HEIGHT * sizeof(uint32_t)) == 0);

  free(snap);
  free(fb_a);
  free(fb_b);
  agnes_destroy(agn);
  free(rom);

  if (!match || nz < 1000) {
    printf("[HOST_CORE] NES_AGNES STATUS=FAIL match=%d non_zero=%d\n", match, nz);
    return 1;
  }
  printf("[HOST_CORE] NES_AGNES STATUS=PASS match=%d non_zero=%d\n", match, nz);
  return 0;
}

int main(void) {
  int failures = 0;
  failures += test_atari_core_cpp();
  failures += test_colem_core();
  failures += test_lynx_core_cpp();
  failures += test_ngp_core();
  failures += test_snes_core();
  failures += test_wswan_core();
  failures += test_genesis_core();
  failures += test_sms_core();
  failures += test_pce_core();
  failures += test_pico_core_cpp();
  failures += test_nes_agnes_core();

  if (failures != 0) {
    printf("FAILED: %d host emulator core check(s) failed.\n", failures);
    return 1;
  }
  printf("ALL 11 HOST EMULATOR CORES (INCLUDING NES AGNES + STATE DETERMINISM) PASSED!\n");
  return 0;
}
