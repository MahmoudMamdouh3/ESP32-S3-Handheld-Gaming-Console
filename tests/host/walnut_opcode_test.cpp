#include "test_check.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#define ENABLE_SOUND 0
#define WALNUT_FULL_GBC_SUPPORT 1
#define WALNUT_GB_RGB565_BIGENDIAN 0

#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include "third_party/walnut_cgb/walnut_cgb.h"

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace {

struct WalnutCtx {
  const uint8_t* rom = nullptr;
  size_t romSize = 0U;
  std::vector<uint8_t> cartRam;
  std::vector<uint8_t> serialOut;
  uint32_t scanlines = 0U;
  int errors = 0;
};

uint8_t romRead8(struct gb_s* gb, const uint_fast32_t addr) {
  auto* ctx = static_cast<WalnutCtx*>(gb->direct.priv);
  return (ctx && addr < ctx->romSize) ? ctx->rom[addr] : 0xFFU;
}

uint16_t romRead16(struct gb_s* gb, const uint_fast32_t addr) {
  return static_cast<uint16_t>(romRead8(gb, addr)) | (static_cast<uint16_t>(romRead8(gb, addr + 1U)) << 8);
}

uint32_t romRead32(struct gb_s* gb, const uint_fast32_t addr) {
  return static_cast<uint32_t>(romRead8(gb, addr)) | (static_cast<uint32_t>(romRead8(gb, addr + 1U)) << 8) |
         (static_cast<uint32_t>(romRead8(gb, addr + 2U)) << 16) |
         (static_cast<uint32_t>(romRead8(gb, addr + 3U)) << 24);
}

uint8_t ramRead8(struct gb_s* gb, const uint_fast32_t addr) {
  auto* ctx = static_cast<WalnutCtx*>(gb->direct.priv);
  return (ctx && addr < ctx->cartRam.size()) ? ctx->cartRam[addr] : 0xFFU;
}

void ramWrite8(struct gb_s* gb, const uint_fast32_t addr, const uint8_t val) {
  auto* ctx = static_cast<WalnutCtx*>(gb->direct.priv);
  if (ctx && addr < ctx->cartRam.size()) ctx->cartRam[addr] = val;
}

void onError(struct gb_s* gb, const enum gb_error_e, const uint16_t) {
  auto* ctx = static_cast<WalnutCtx*>(gb->direct.priv);
  if (ctx) ctx->errors++;
}

void onSerialTx(struct gb_s* gb, const uint8_t tx) {
  auto* ctx = static_cast<WalnutCtx*>(gb->direct.priv);
  if (ctx) ctx->serialOut.push_back(tx);
}

void onLcdLine(struct gb_s* gb, const uint8_t* pixels, const uint_fast8_t line) {
  (void)pixels;
  (void)line;
  auto* ctx = static_cast<WalnutCtx*>(gb->direct.priv);
  if (ctx) ctx->scanlines++;
}

void finalizeCartridge(std::vector<uint8_t>& rom) {
  const char* title = "WALNUT_SM83";
  for (size_t i = 0; title[i] != '\0' && i < 15U; ++i) {
    rom[0x0134U + i] = static_cast<uint8_t>(title[i]);
  }
  rom[0x0143U] = 0xC0U; // GBC-only flag so cgbMode activates cleanly
  uint8_t sum = 0U;
  for (uint16_t a = 0x0134U; a <= 0x014CU; ++a) {
    sum = static_cast<uint8_t>(sum - rom[a] - 1U);
  }
  rom[0x014DU] = sum;
}

int testWalnutWramBankingAndUnalignedBulkOps() {
  std::vector<uint8_t> rom(32768U, 0x00U);
  rom[0x0100U] = 0x00U; // NOP
  rom[0x0101U] = 0xC3U; // JP 0x0150
  rom[0x0102U] = 0x50U;
  rom[0x0103U] = 0x01U;

  // Program enables LCD + selects SVBK bank 2 + emits 0x7E over serial
  size_t pc = 0x0150U;
  rom[pc++] = 0x3EU;
  rom[pc++] = 0x81U;
  rom[pc++] = 0xE0U;
  rom[pc++] = 0x40U; // LDH (LCDC), 0x81
  rom[pc++] = 0x3EU;
  rom[pc++] = 0x02U;
  rom[pc++] = 0xE0U;
  rom[pc++] = 0x70U; // LDH (SVBK), 0x02
  rom[pc++] = 0x3EU;
  rom[pc++] = 0x7EU;
  rom[pc++] = 0xE0U;
  rom[pc++] = 0x01U; // LDH (SB), 0x7E
  rom[pc++] = 0x3EU;
  rom[pc++] = 0x81U;
  rom[pc++] = 0xE0U;
  rom[pc++] = 0x02U; // LDH (SC), 0x81
  rom[pc++] = 0x18U;
  rom[pc++] = 0xFEU; // JR -2

  finalizeCartridge(rom);

  WalnutCtx ctx;
  ctx.rom = rom.data();
  ctx.romSize = rom.size();

  struct gb_s gb;
  memset(&gb, 0, sizeof(gb));
  BMO_CHECK(gb_init(&gb, romRead8, romRead16, romRead32, ramRead8, ramWrite8, onError, &ctx) == GB_INIT_NO_ERROR);
  gb_init_lcd(&gb, onLcdLine);
  gb_init_serial(&gb, onSerialTx, nullptr);

  // Run frames so ROM sets GBC mode + SVBK bank 2
  for (int f = 0; f < 4; ++f) {
    gb_run_frame(&gb);
  }
  BMO_CHECK(ctx.errors == 0);
  BMO_CHECK(ctx.scanlines >= 144U);
  BMO_CHECK(ctx.serialOut.size() == 1U && ctx.serialOut[0] == 0x7EU);
  BMO_CHECK(gb.cgb.cgbMode == 1);

  // Switch explicitly to WRAM Bank 2 and exercise 16/32-bit bulk writes & reads on Bank 0 (0xC001) vs Bank 2 (0xD001)
  __gb_write(&gb, 0xFF70U, 2U);
  __gb_write32(&gb, 0xD001U, 0x11223344U);
  __gb_write32(&gb, 0xC001U, 0xAABBCCDDU);

  // Writing Bank 0 at 0xC001 must NOT touch Bank 2 (0xD001) or Bank 1 (0x2000-offset)!
  BMO_CHECK(__gb_read32(&gb, 0xC001U) == 0xAABBCCDDU);
  BMO_CHECK(__gb_read32(&gb, 0xD001U) == 0x11223344U);

  __gb_write16(&gb, 0xC003U, 0x5566U);
  BMO_CHECK(__gb_read16(&gb, 0xC003U) == 0x5566U);
  BMO_CHECK(__gb_read32(&gb, 0xD001U) == 0x11223344U);

  // Switching back to bank 1 shows clean separate bank data and unchanged bank 0
  __gb_write(&gb, 0xFF70U, 1U);
  BMO_CHECK(__gb_read32(&gb, 0xC001U) == 0x5566CCDDU);
  BMO_CHECK(__gb_read32(&gb, 0xD001U) == 0x00000000U);

  // Trigger OAM DMA from unaligned-compatible WRAM page 0xC0 -> copies 0xC000..0xC09F to OAM
  __gb_write(&gb, 0xFF46U, 0xC0U);
  BMO_CHECK(gb.oam[1] == 0xDDU);
  BMO_CHECK(gb.oam[2] == 0xCCU);
  return 0;
}

} // namespace

int main() {
  return testWalnutWramBankingAndUnalignedBulkOps();
}
