/* Host execution harness for Snes9x 2005 (SNES). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "snes9x.h"
#include "src/dsp.h"

void S9xSetLineCallback(S9xLineCallback cb);

static uint16_t* s_snes_fb = NULL;
static uint32_t s_apu_rand = 0xA55A1234U;
static bool s_snes_running = false;

static void snes_line_cb(uint32_t y, const uint16_t* pixels, uint32_t width) {
  if (!s_snes_fb || !pixels || y >= 224U || width == 0U) return;
  uint16_t* dst = s_snes_fb + (size_t)y * 256U;
  uint32_t copy_w = (width > 256U) ? 256U : width;
  for (uint32_t x = 0; x < copy_w; ++x) {
    dst[x] = (width > 256U) ? pixels[x * 2U] : pixels[x];
  }
}

bool S9xInitDisplay(void) {
  GFX.Pitch = SNES_WIDTH * sizeof(uint16_t);
  GFX.Pitch2 = GFX.Pitch;
  GFX.RealPitch = GFX.Pitch;
  GFX.ZPitch = SNES_WIDTH;
  GFX.PPL = SNES_WIDTH;
  GFX.PPLx2 = SNES_WIDTH * 2U;
  GFX.Screen = NULL;
  GFX.SubScreen = NULL;
  GFX.ZBuffer = NULL;
  GFX.SubZBuffer = NULL;
  GFX.LineRenderMode = true;
  GFX.LinePPL = SNES_WIDTH;
  GFX.LinePitch = SNES_WIDTH * sizeof(uint16_t);
  return true;
}

void S9xDeinitDisplay(void) {}
void S9xExtraUsage(void) {}
void S9xExit(void) {
  s_snes_running = false;
}
void S9xMessage(int type, int number, const char* message) {
  (void)type;
  (void)number;
  (void)message;
}
bool S9xIsSourceLineNeeded(uint32_t srcY) {
  return srcY < 224U;
}
uint32_t S9xReadJoypad(int32_t port) {
  (void)port;
  return 0U;
}
bool S9xReadMousePosition(int32_t which1, int32_t* x, int32_t* y, uint32_t* buttons) {
  (void)which1;
  (void)x;
  (void)y;
  (void)buttons;
  return false;
}
bool S9xReadSuperScopePosition(int32_t* x, int32_t* y, uint32_t* buttons) {
  (void)x;
  (void)y;
  (void)buttons;
  return false;
}
bool JustifierOffscreen(void) {
  return true;
}
void JustifierButtons(uint32_t* justifiers) {
  (void)justifiers;
}

void S9xAPUWritePort(int32_t Address, uint8_t Byte) {
  if (Memory.FillRAM) {
    Memory.FillRAM[(uint16_t)(0x2140U + (Address & 3))] = Byte;
  }
}

uint8_t S9xAPUReadPort(int32_t Address) {
  s_apu_rand = s_apu_rand * 1664525U + 1013904223U;
  int32_t p = Address & 3;
  if (p < 2 && ((s_apu_rand & 6U) != 0U)) {
    return (p == 1) ? 0xAAU : 0xBBU;
  }
  return Memory.FillRAM ? Memory.FillRAM[(uint16_t)(0x2140U + p)] : 0U;
}

void S9xResetAPU(void) {
  if (Memory.FillRAM) {
    Memory.FillRAM[0x2140] = 0xAAU;
    Memory.FillRAM[0x2141] = 0xBBU;
    Memory.FillRAM[0x2142] = 0x00U;
    Memory.FillRAM[0x2143] = 0x00U;
  }
}

int test_snes_core(void) {
  uint8_t* rom = (uint8_t*)calloc(1U, 0x8000U);
  s_snes_fb = (uint16_t*)calloc(256U * 224U, sizeof(uint16_t));
  if (!rom || !s_snes_fb) {
    free(rom);
    free(s_snes_fb);
    s_snes_fb = NULL;
    printf("[HOST_CORE] SNES_65C816 STATUS=FAIL (oom)\n");
    return 1;
  }

  /* 65C816 LoROM program at $8000 (offset 0x0000): unblank display, set CRAM[0] green, store WRAM[0]=0x42. */
  const uint8_t prog[] = {
      0x78U,                             /* SEI */
      0x18U, 0xFBU,                      /* CLC ; XCE */
      0xA9U, 0x42U, 0x8DU, 0x00U, 0x00U, /* LDA #$42 ; STA $0000 */
      0xA9U, 0x0FU, 0x8DU, 0x00U, 0x21U, /* LDA #$0F ; STA $2100 */
      0xA9U, 0x00U, 0x8DU, 0x21U, 0x21U, /* LDA #$00 ; STA $2121 */
      0xA9U, 0xE0U, 0x8DU, 0x22U, 0x21U, /* LDA #$E0 ; STA $2122 */
      0xA9U, 0x03U, 0x8DU, 0x22U, 0x21U, /* LDA #$03 ; STA $2122 */
      0x80U, 0xFEU                       /* BRA -2 */
  };
  memcpy(rom, prog, sizeof(prog));

  memcpy(&rom[0x7FC0], "BMO SNES HOST TEST   ", 21);
  rom[0x7FD5] = 0x20U; /* LoROM */
  rom[0x7FD6] = 0x00U; /* ROM only */
  rom[0x7FD7] = 0x05U; /* 32 KB */
  rom[0x7FD8] = 0x00U;
  rom[0x7FD9] = 0x01U; /* NTSC */
  rom[0x7FFC] = 0x00U; /* Reset vector -> $8000 */
  rom[0x7FFD] = 0x80U;

  uint32_t sum = 0;
  for (size_t i = 0; i < 0x8000U; ++i) {
    if (i >= 0x7FDCU && i <= 0x7FDFU) continue;
    sum += rom[i];
  }
  sum += 0xFFU + 0xFFU;
  uint16_t csum = (uint16_t)(sum & 0xFFFFU);
  uint16_t comp = (uint16_t)(csum ^ 0xFFFFU);
  rom[0x7FDC] = (uint8_t)(comp & 0xFFU);
  rom[0x7FDD] = (uint8_t)(comp >> 8);
  rom[0x7FDE] = (uint8_t)(csum & 0xFFU);
  rom[0x7FDF] = (uint8_t)(csum >> 8);

  memset(&Settings, 0, sizeof(Settings));
  Settings.CyclesPercentage = 100;
  Settings.H_Max = SNES_CYCLES_PER_SCANLINE;
  Settings.FrameTimePAL = 20000;
  Settings.FrameTimeNTSC = 16667;
  Settings.ControllerOption = SNES_JOYPAD;
  Settings.HBlankStart = (256 * Settings.H_Max) / SNES_HCOUNTER_MAX;
  Settings.Mute = true;
  Settings.APUEnabled = false;

  Memory.ROM = rom;
  Memory.ROM_Offset = 0;
  Memory.ROM_AllocSize = 0x8000U;

  if (!S9xInitDisplay() || !S9xInitMemory() || !S9xInitGFX() || !S9xInitMap() || !S9xInitPpu() ||
      !S9xInitLineBuffers() || !LoadROM(NULL)) {
    S9xSetLineCallback(NULL);
    S9xDeinitDSP();
    S9xDeinitGFX();
    S9xDeinitMemory();
    S9xDeinitDisplay();
    free(rom);
    free(s_snes_fb);
    s_snes_fb = NULL;
    printf("[HOST_CORE] SNES_65C816 STATUS=FAIL (init)\n");
    return 1;
  }

  S9xSetLineCallback(snes_line_cb);
  S9xFixColourBrightness();
  S9xReset();
  s_snes_running = true;

  for (int f = 0; f < 50; ++f) {
    IPPU.RenderThisFrame = true;
    S9xMainLoop();
  }

  uint8_t wram0 = Memory.RAM ? Memory.RAM[0] : 0U;
  int nz = 0;
  for (size_t i = 0; i < 256U * 224U; ++i) {
    if (s_snes_fb[i] != 0U) ++nz;
  }

  s_snes_running = false;
  S9xSetLineCallback(NULL);
  S9xDeinitDSP();
  S9xDeinitGFX();
  S9xDeinitMemory();
  S9xDeinitDisplay();
  free(rom);
  free(s_snes_fb);
  s_snes_fb = NULL;

  if (wram0 != 0x42U || nz < 1000) {
    printf("[HOST_CORE] SNES_65C816 STATUS=FAIL wram0=0x%02X non_zero=%d\n", wram0, nz);
    return 1;
  }
  printf("[HOST_CORE] SNES_65C816 STATUS=PASS wram0=0x%02X non_zero=%d\n", wram0, nz);
  return 0;
}
