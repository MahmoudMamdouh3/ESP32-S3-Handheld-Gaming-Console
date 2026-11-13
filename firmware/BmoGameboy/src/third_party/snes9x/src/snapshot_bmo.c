#include <string.h>
#include "snapshot.h"
#include "snes9x.h"
#include "memmap.h"
#include "ppu.h"
#include "cpuexec.h"
#include "dma.h"

static const uint8_t kBmoS9xMagic[8] = { 'B', 'M', 'O', 'S', '9', 'X', '0', '1' };

size_t S9xBmoSerializeSize(void)
{
   return sizeof(kBmoS9xMagic)
        + sizeof(CPU) + sizeof(ICPU) + sizeof(PPU) + sizeof(DMA)
        + (256 * sizeof(uint16_t)) + (128 * sizeof(SOBJ)) + (512 + 32)
        + VRAM_SIZE + RAM_SIZE + FILLRAM_SIZE;
}

bool S9xBmoSerialize(void *dst, size_t maxBytes, size_t *outBytes)
{
   size_t need = S9xBmoSerializeSize();
   if (!dst || maxBytes < need || !Memory.VRAM || !Memory.RAM || !Memory.FillRAM ||
       !PPU.CGDATA || !PPU.OBJ || !PPU.OAMData)
      return false;
   uint8_t *w = (uint8_t *)dst;
   memcpy(w, kBmoS9xMagic, sizeof(kBmoS9xMagic));         w += sizeof(kBmoS9xMagic);
   memcpy(w, &CPU, sizeof(CPU));                          w += sizeof(CPU);
   memcpy(w, &ICPU, sizeof(ICPU));                        w += sizeof(ICPU);
   memcpy(w, &PPU, sizeof(PPU));                          w += sizeof(PPU);
   memcpy(w, &DMA, sizeof(DMA));                          w += sizeof(DMA);
   memcpy(w, PPU.CGDATA, 256 * sizeof(uint16_t));         w += 256 * sizeof(uint16_t);
   memcpy(w, PPU.OBJ, 128 * sizeof(SOBJ));                w += 128 * sizeof(SOBJ);
   memcpy(w, PPU.OAMData, 512 + 32);                      w += 512 + 32;
   memcpy(w, Memory.VRAM, VRAM_SIZE);                     w += VRAM_SIZE;
   memcpy(w, Memory.RAM, RAM_SIZE);                       w += RAM_SIZE;
   memcpy(w, Memory.FillRAM, FILLRAM_SIZE);               w += FILLRAM_SIZE;
   if (outBytes) *outBytes = need;
   return true;
}

bool S9xBmoUnserialize(const void *src, size_t bytes)
{
   size_t need = S9xBmoSerializeSize();
   if (!src || bytes < need || !Memory.VRAM || !Memory.RAM || !Memory.FillRAM ||
       !PPU.CGDATA || !PPU.OBJ || !PPU.OAMData)
      return false;
   const uint8_t *r = (const uint8_t *)src;
   if (memcmp(r, kBmoS9xMagic, sizeof(kBmoS9xMagic)) != 0)
      return false;
   r += sizeof(kBmoS9xMagic);
   uint16_t *savedCgData = PPU.CGDATA;
   SOBJ     *savedObj    = PPU.OBJ;
   uint8_t  *savedOam    = PPU.OAMData;
   memcpy(&CPU, r, sizeof(CPU));                          r += sizeof(CPU);
   memcpy(&ICPU, r, sizeof(ICPU));                        r += sizeof(ICPU);
   memcpy(&PPU, r, sizeof(PPU));                          r += sizeof(PPU);
   memcpy(&DMA, r, sizeof(DMA));                          r += sizeof(DMA);
   PPU.CGDATA  = savedCgData;
   PPU.OBJ     = savedObj;
   PPU.OAMData = savedOam;
   memcpy(PPU.CGDATA, r, 256 * sizeof(uint16_t));         r += 256 * sizeof(uint16_t);
   memcpy(PPU.OBJ, r, 128 * sizeof(SOBJ));                r += 128 * sizeof(SOBJ);
   memcpy(PPU.OAMData, r, 512 + 32);                      r += 512 + 32;
   memcpy(Memory.VRAM, r, VRAM_SIZE);                     r += VRAM_SIZE;
   memcpy(Memory.RAM, r, RAM_SIZE);                       r += RAM_SIZE;
   memcpy(Memory.FillRAM, r, FILLRAM_SIZE);               r += FILLRAM_SIZE;
   FixROMSpeed();
   IPPU.ColorsChanged = true;
   IPPU.OBJChanged    = true;
   CPU.InDMA          = false;
   CPU.WaitAddress    = NULL;
   CPU.PCAtOpcodeStart = NULL;
   S9xFixColourBrightness();
   ICPU.ShiftedPB     = ICPU.Registers.PB << 16;
   ICPU.ShiftedDB     = ICPU.Registers.DB << 16;
   S9xSetPCBase(ICPU.ShiftedPB + ICPU.Registers.PC);
   S9xUnpackStatus();
   S9xFixCycles();
   S9xReschedule();
   return true;
}
