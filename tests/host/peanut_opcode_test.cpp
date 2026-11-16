#include "test_check.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "third_party/peanut_gb/peanut_gb_config.h"

extern "C" uint8_t audio_read(uint16_t) {
  return 0;
}

extern "C" void audio_write(uint16_t, uint8_t) {}

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

namespace PGB {
#include "third_party/peanut_gb/peanut_gb.h"
}

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace {

struct HarnessCtx {
  const uint8_t* rom = nullptr;
  size_t romSize = 0U;
  std::vector<uint8_t> cartRam;
  std::vector<uint8_t> serialOut;
  uint32_t scanlinesDrawn = 0U;
  uint32_t nonZeroPixels = 0U;
  int errorCount = 0;
};

uint8_t romRead(PGB::gb_s* gb, const uint_fast32_t addr) {
  auto* ctx = static_cast<HarnessCtx*>(gb->direct.priv);
  return (ctx && addr < ctx->romSize) ? ctx->rom[addr] : 0xFFU;
}

uint8_t ramRead(PGB::gb_s* gb, const uint_fast32_t addr) {
  auto* ctx = static_cast<HarnessCtx*>(gb->direct.priv);
  return (ctx && addr < ctx->cartRam.size()) ? ctx->cartRam[addr] : 0xFFU;
}

void ramWrite(PGB::gb_s* gb, const uint_fast32_t addr, const uint8_t val) {
  auto* ctx = static_cast<HarnessCtx*>(gb->direct.priv);
  if (ctx && addr < ctx->cartRam.size()) ctx->cartRam[addr] = val;
}

void gbError(PGB::gb_s* gb, const enum PGB::gb_error_e, const uint16_t) {
  auto* ctx = static_cast<HarnessCtx*>(gb->direct.priv);
  if (ctx) ctx->errorCount++;
}

void serialTx(PGB::gb_s* gb, const uint8_t tx) {
  auto* ctx = static_cast<HarnessCtx*>(gb->direct.priv);
  if (ctx) ctx->serialOut.push_back(tx);
}

void lcdDrawLine(PGB::gb_s* gb, const uint8_t pixels[LCD_WIDTH], const uint_fast8_t line) {
  (void)line;
  auto* ctx = static_cast<HarnessCtx*>(gb->direct.priv);
  if (!ctx) return;
  ctx->scanlinesDrawn++;
  for (int x = 0; x < LCD_WIDTH; ++x) {
    if ((pixels[x] & 0x03U) != 0U) ctx->nonZeroPixels++;
  }
}

void finalizeCartridgeChecksum(std::vector<uint8_t>& rom, const char* title) {
  for (size_t i = 0; title[i] != '\0' && i < 15U; ++i) {
    rom[0x0134U + i] = static_cast<uint8_t>(title[i]);
  }
  uint8_t sum = 0U;
  for (uint16_t a = 0x0134U; a <= 0x014CU; ++a) {
    sum = static_cast<uint8_t>(sum - rom[a] - 1U);
  }
  rom[0x014DU] = sum;
}

void emitPushCall(std::vector<uint8_t>& rom, size_t& pc, uint16_t addr) {
  rom[pc++] = 0xCDU; // CALL nn
  rom[pc++] = static_cast<uint8_t>(addr & 0xFFU);
  rom[pc++] = static_cast<uint8_t>((addr >> 8) & 0xFFU);
}

std::vector<uint8_t> buildSyntheticSm83Rom() {
  std::vector<uint8_t> rom(32768U, 0x00U);
  rom[0x0100U] = 0x00U; // NOP
  rom[0x0101U] = 0xC3U; // JP 0x0150
  rom[0x0102U] = 0x50U;
  rom[0x0103U] = 0x01U;

  // Subroutine emitSerial(A) at 0x01B0
  size_t subTx = 0x01B0U;
  rom[subTx++] = 0xE0U;
  rom[subTx++] = 0x01U;
  rom[subTx++] = 0x3EU;
  rom[subTx++] = 0x81U;
  rom[subTx++] = 0xE0U;
  rom[subTx++] = 0x02U;
  rom[subTx++] = 0xF0U;
  rom[subTx++] = 0x02U;
  rom[subTx++] = 0xE6U;
  rom[subTx++] = 0x80U;
  rom[subTx++] = 0x20U;
  rom[subTx++] = 0xFAU;
  rom[subTx++] = 0xC9U;

  // Subroutine loopSum() at 0x01D0
  size_t subSum = 0x01D0U;
  rom[subSum++] = 0x06U;
  rom[subSum++] = 0x04U;
  rom[subSum++] = 0xAFU;
  rom[subSum++] = 0xC6U;
  rom[subSum++] = 0x10U;
  rom[subSum++] = 0x05U;
  rom[subSum++] = 0x20U;
  rom[subSum++] = 0xFBU;
  rom[subSum++] = 0xC9U;

  size_t pc = 0x0150U;
  rom[pc++] = 0x31U;
  rom[pc++] = 0xFEU;
  rom[pc++] = 0xDFU;
  rom[pc++] = 0x3EU;
  rom[pc++] = 0xE4U;
  rom[pc++] = 0xE0U;
  rom[pc++] = 0x47U;
  rom[pc++] = 0x3EU;
  rom[pc++] = 0x81U;
  rom[pc++] = 0xE0U;
  rom[pc++] = 0x40U;

  // 1) DAA BCD check: 0x29 + 0x17 + DAA -> 0x46
  rom[pc++] = 0x3EU;
  rom[pc++] = 0x29U;
  rom[pc++] = 0xC6U;
  rom[pc++] = 0x17U;
  rom[pc++] = 0x27U;
  emitPushCall(rom, pc, 0x01B0U);

  // 2) Carry branch check -> 0xA5
  rom[pc++] = 0x3EU;
  rom[pc++] = 0x80U;
  rom[pc++] = 0xC6U;
  rom[pc++] = 0x85U;
  rom[pc++] = 0x30U;
  rom[pc++] = 0x02U;
  rom[pc++] = 0x3EU;
  rom[pc++] = 0xA5U;
  emitPushCall(rom, pc, 0x01B0U);

  // 3) CALL/RET + DJNZ loop -> 0x40
  emitPushCall(rom, pc, 0x01D0U);
  emitPushCall(rom, pc, 0x01B0U);

  // 4) Joypad input matrix P15=0 poll loop
  const size_t spinPc = pc;
  rom[pc++] = 0x3EU;
  rom[pc++] = 0x10U;
  rom[pc++] = 0xE0U;
  rom[pc++] = 0x00U;
  rom[pc++] = 0xF0U;
  rom[pc++] = 0x00U;
  rom[pc++] = 0xE6U;
  rom[pc++] = 0x0FU;
  rom[pc++] = 0xFEU;
  rom[pc++] = 0x0FU;
  rom[pc++] = 0x28U;
  const uint8_t relOffset = static_cast<uint8_t>((spinPc - (pc + 1U)) & 0xFFU);
  rom[pc++] = relOffset;
  emitPushCall(rom, pc, 0x01B0U);
  rom[pc++] = 0x18U;
  rom[pc++] = 0xFEU;

  finalizeCartridgeChecksum(rom, "BMO_SM83_TEST");
  return rom;
}

int testSyntheticSm83Opcodes() {
  const std::vector<uint8_t> rom = buildSyntheticSm83Rom();

  HarnessCtx ctx;
  ctx.rom = rom.data();
  ctx.romSize = rom.size();

  PGB::gb_s gb;
  memset(&gb, 0, sizeof(gb));
  const PGB::gb_init_error_e initErr = PGB::gb_init(&gb, romRead, ramRead, ramWrite, gbError, &ctx);
  BMO_CHECK(initErr == PGB::GB_INIT_NO_ERROR);
  PGB::gb_init_lcd(&gb, lcdDrawLine);
  PGB::gb_init_serial(&gb, serialTx, nullptr);
  gb.direct.joypad = 0xFFU;

  for (int f = 0; f < 3; ++f) {
    PGB::gb_run_frame(&gb);
  }
  BMO_CHECK(ctx.errorCount == 0);
  BMO_CHECK(ctx.scanlinesDrawn >= 144U * 2U);
  BMO_CHECK(ctx.serialOut.size() == 3U);
  BMO_CHECK(ctx.serialOut[0] == 0x46U);
  BMO_CHECK(ctx.serialOut[1] == 0xA5U);
  BMO_CHECK(ctx.serialOut[2] == 0x40U);

  gb.direct.joypad = static_cast<uint8_t>(0xFFU ^ JOYPAD_A);
  for (int f = 0; f < 2; ++f) {
    PGB::gb_run_frame(&gb);
  }
  BMO_CHECK(ctx.errorCount == 0);
  BMO_CHECK(ctx.serialOut.size() == 4U);
  BMO_CHECK(ctx.serialOut[3] == 0x0EU);
  return 0;
}

int testOptionalBlarggCpuInstrs() {
#ifdef BMO_REPO_ROOT_DIR
  const std::string fixturePath = std::string(BMO_REPO_ROOT_DIR) + "/tests/fixtures/cpu_instrs.gb";
  FILE* fp = fopen(fixturePath.c_str(), "rb");
  if (!fp) return 0; // Fixture is gitignored on clean clones; skip silently when absent

  fseek(fp, 0, SEEK_END);
  const long sz = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  BMO_CHECK(sz >= 32768L && sz <= 2097152L);
  std::vector<uint8_t> rom(static_cast<size_t>(sz));
  BMO_CHECK(fread(rom.data(), 1, rom.size(), fp) == rom.size());
  fclose(fp);

  HarnessCtx ctx;
  ctx.rom = rom.data();
  ctx.romSize = rom.size();
  ctx.cartRam.assign(32768U, 0x00U);

  PGB::gb_s gb;
  memset(&gb, 0, sizeof(gb));
  BMO_CHECK(PGB::gb_init(&gb, romRead, ramRead, ramWrite, gbError, &ctx) == PGB::GB_INIT_NO_ERROR);
  PGB::gb_init_lcd(&gb, lcdDrawLine);
  PGB::gb_init_serial(&gb, serialTx, nullptr);
  gb.direct.joypad = 0xFFU;

  // Step 400 frames (~6.7 s simulated GB time) — enough for boot + early subtests without slowing CTest
  for (int f = 0; f < 400; ++f) {
    PGB::gb_run_frame(&gb);
  }
  BMO_CHECK(ctx.errorCount == 0);
  const std::string out(ctx.serialOut.begin(), ctx.serialOut.end());
  BMO_CHECK(out.find("cpu_instrs") != std::string::npos);
#endif
  return 0;
}

} // namespace

int main() {
  if (testSyntheticSm83Opcodes() != 0) return 1;
  if (testOptionalBlarggCpuInstrs() != 0) return 1;
  return 0;
}
