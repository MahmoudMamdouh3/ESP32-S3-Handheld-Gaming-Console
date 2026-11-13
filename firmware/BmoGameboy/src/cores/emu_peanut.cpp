#pragma GCC optimize("O3,unroll-loops")
#include "emu_peanut.h"
#include "../hal/buttons.h"
#include "../ui/display_emu.h"
#include "../ui/bmo_face.h"
#include "../hal/config.h"
#include "../storage/save_manager.h"
#include "../storage/battery_save.h"
#include <string.h>
#include <Arduino.h>
#include <esp_heap_caps.h>

#include "../third_party/peanut_gb/peanut_gb_config.h"

#include <stdint.h>
#include <stdlib.h>

#if FEATURE_AUDIO
extern "C" __attribute__((weak)) uint8_t audio_read(const uint16_t addr) {
  return 0; // Replace with actual APU read if minigb_apu is integrated
}
extern "C" __attribute__((weak)) void audio_write(const uint16_t addr, const uint8_t val) {
  // Replace with actual APU write / I2S push
}
#else
extern "C" __attribute__((weak)) uint8_t audio_read(const uint16_t addr) {
  return 0;
}
extern "C" __attribute__((weak)) void audio_write(const uint16_t addr, const uint8_t val) {}
#endif

namespace PGB {
#include "../third_party/peanut_gb/peanut_gb.h"
}
using namespace PGB;

namespace {
// Stub audio callbacks moved above

// E3: Align the emulator state struct to the ESP32-S3 D-cache line
// size (32 bytes). Prevents cache thrashing on hot registers.
static struct gb_s* gb = nullptr;

const uint8_t* current_rom_data = nullptr;
size_t current_rom_len = 0;

// Keep cartridge save RAM out of scarce internal DRAM.  128 KB covers the
// larger MBC5/CGB save configurations; PSRAM latency is acceptable here
// because cartridge-RAM access is far less frequent than CPU registers.
static constexpr size_t CART_RAM_SIZE = 128 * 1024;
static uint8_t* cart_ram = nullptr;

static size_t batteryRamSize() {
  if (!gb) return 0;
  const uint8_t type = gb->gb_rom_read ? gb->gb_rom_read(gb, 0x0147) : 0;
  const uint8_t sizeCode = gb->gb_rom_read ? gb->gb_rom_read(gb, 0x0149) : 0;
  const size_t size = BatterySave::gameBoyBatteryBytes(type, sizeCode);
  return size <= CART_RAM_SIZE ? size : 0;
}

// N5: Module-level, 4-byte aligned — avoids BSS contention with gb_s data.
static uint16_t __attribute__((aligned(4))) rowBuffer[240];
static uint16_t* s_asyncGbFrame = nullptr;

static uint16_t PAL_256[256];

// N1: IRAM_ATTR — these callbacks are invoked ~280K times per frame by the
// emulator core; zero-wait-state IRAM placement eliminates I-cache stalls.

IRAM_ATTR uint8_t gb_rom_read(struct gb_s* gbCtx, const uint_fast32_t addr) {
  (void)gbCtx;
  if (addr >= current_rom_len) return 0xFF;
  return current_rom_data[addr];
}

IRAM_ATTR uint8_t gb_cart_ram_read(struct gb_s* gbCtx, const uint_fast32_t addr) {
  (void)gbCtx;
  if (!cart_ram || addr >= CART_RAM_SIZE) {
    static bool warned = false;
    if (!warned) {
      LOG_WARN_STR("WARNING: Cart RAM read overflow!");
      warned = true;
    }
    return 0xFF;
  }
  return cart_ram[addr];
}

IRAM_ATTR void gb_cart_ram_write(struct gb_s* gbCtx, const uint_fast32_t addr, const uint8_t val) {
  (void)gbCtx;
  if (!cart_ram || addr >= CART_RAM_SIZE) {
    static bool warned = false;
    if (!warned) {
      LOG_WARN_STR("WARNING: Cart RAM write overflow!");
      warned = true;
    }
    return;
  }
  cart_ram[addr] = val;
}

void gb_error(struct gb_s* gbCtx, const enum gb_error_e gb_err, const uint16_t val) {
  (void)gbCtx;
  LOG_ERROR("Peanut-GB Error: %d at PC 0x%04X", gb_err, val);
  Serial.flush();
  BmoFace::setExpression(BmoFace::ERROR);
  BmoFace::draw(); // Force draw immediately before restart
  esp_restart();
}

// N1: lcd_draw_line in IRAM — called 144×/frame.
IRAM_ATTR void lcd_draw_line(struct gb_s* gbCtx, const uint8_t pixels[160], const uint_fast8_t line) {
  (void)gbCtx;
  // E1: Process 4 source pixels per iteration → 6 output pixels.
  // This perfectly aligns to three 32-bit memory stores (12 bytes),
  // eliminating unaligned store penalties on the Xtensa LX7 CPU.
  uint32_t* out32 = (uint32_t*)rowBuffer;
  for (int g = 0; g < 40; g++) {
    uint16_t pA = PAL_256[pixels[g * 4]];
    uint16_t pB = PAL_256[pixels[g * 4 + 1]];
    uint16_t pC = PAL_256[pixels[g * 4 + 2]];
    uint16_t pD = PAL_256[pixels[g * 4 + 3]];

    out32[0] = (uint32_t)pA | ((uint32_t)pA << 16);
    out32[1] = (uint32_t)pB | ((uint32_t)pC << 16);
    out32[2] = (uint32_t)pC | ((uint32_t)pD << 16);

    out32 += 3;
  }

  // N3: Scale into inactive PSRAM ping-pong framebuffer so Core 0 DisplayWorker
  // can DMA-stream this frame concurrently while Core 1 starts the next frame.
  // Exact 3:2 nearest-neighbour scale (A B C D -> A A B C C D), duplicating even source rows.
  const int rows_to_draw = (line & 1u) ? 1 : 2;
  const int y0 = (static_cast<int>(line >> 1) * 3) + ((line & 1u) ? 2 : 0);
  if (s_asyncGbFrame && line < 144U) {
    uint16_t* dstRow = &s_asyncGbFrame[y0 * 240];
    memcpy(dstRow, rowBuffer, 240U * sizeof(uint16_t));
    if (rows_to_draw == 2) {
      memcpy(dstRow + 240, rowBuffer, 240U * sizeof(uint16_t));
    }
  }
}
} // namespace

bool PeanutEmu::init(const uint8_t* rom_data, size_t rom_len) {
  destroy();
  current_rom_data = rom_data;
  current_rom_len = rom_len;

  // Precompute O(1) 256-entry palette lookup table to save bitwise ops in hot loop
  for (int i = 0; i < 256; i++) {
    PAL_256[i] = DisplayEmu::CLASSIC_PALETTE[i & 0x03];
  }

  gb = (struct gb_s*)heap_caps_aligned_alloc(32, sizeof(struct gb_s), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!gb) {
    gb = (struct gb_s*)heap_caps_aligned_alloc(32, sizeof(struct gb_s), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (!gb) {
    LOG_ERROR_STR("Peanut-GB: unable to allocate emulator state.");
    return false;
  }
  memset(gb, 0, sizeof(struct gb_s));

  cart_ram = (uint8_t*)heap_caps_malloc(CART_RAM_SIZE, MALLOC_CAP_SPIRAM);
  if (!cart_ram) {
    LOG_ERROR_STR("Peanut-GB: unable to allocate PSRAM cartridge RAM.");
    destroy();
    return false;
  }

  // Clear cart RAM to prevent save-data bleed between games.
  memset(cart_ram, 0, CART_RAM_SIZE);

  LOG_INFO_STR("Initializing Peanut-GB...");
  LOG_INFO("ROM loaded: %u bytes", rom_len);

  enum gb_init_error_e ret = gb_init(gb, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write, &gb_error, nullptr);
  if (ret != GB_INIT_NO_ERROR) {
    LOG_ERROR("gb_init() failed: %d", ret);
    destroy();
    return false;
  }

  gb_init_lcd(gb, &lcd_draw_line);

  // Reset joypad so no button fires on the first emulator frame.
  gb->direct.joypad = 0xFF;

  LOG_INFO_STR("Peanut-GB initialized.");
  return true;
}

void PeanutEmu::update() {
  if (!gb) return;
  // NB2: Single branchless bitmask direct copy - zero function call overhead.
  gb->direct.joypad = Buttons::gb_joypad_state;
  static int lastPaletteIdx = -1;
  int curPaletteIdx = DisplayEmu::getDmgPaletteIndex();
  if (curPaletteIdx != lastPaletteIdx) {
    const uint16_t* pal = DisplayEmu::getActiveDmgPalette();
    for (int i = 0; i < 256; i++) {
      PAL_256[i] = pal[i & 0x03];
    }
    lastPaletteIdx = curPaletteIdx;
  }

  s_asyncGbFrame = DisplayEmu::acquireAsyncFrameBuffer();
  if (!s_asyncGbFrame) {
    return;
  }
  gb_run_frame(gb);
  DisplayEmu::submitGBFrameAsync(s_asyncGbFrame);
  s_asyncGbFrame = nullptr;
}

void PeanutEmu::destroy() {
  s_asyncGbFrame = nullptr;
  DisplayEmu::waitForDisplay();
  if (cart_ram) {
    heap_caps_free(cart_ram); // matched to heap_caps_malloc(MALLOC_CAP_SPIRAM)
    cart_ram = nullptr;
  }
  if (gb) {
    heap_caps_free(gb);
    gb = nullptr;
  }
  current_rom_data = nullptr;
  current_rom_len = 0;
}

bool PeanutEmu::saveBatteryRam(const char* romFilename) {
  if (!gb || !cart_ram || !romFilename) return false;
  const size_t size = batteryRamSize();
  if (size == 0) return false;
  return SaveManager::saveBatteryRam(romFilename, cart_ram, size);
}

bool PeanutEmu::loadBatteryRam(const char* romFilename) {
  if (!gb || !cart_ram || !romFilename) return false;
  const size_t size = batteryRamSize();
  if (size == 0) return false;
  return SaveManager::loadBatteryRam(romFilename, cart_ram, size);
}

bool PeanutEmu::saveState(const char* romFilename, int slot) {
  if (!gb || !cart_ram || !romFilename) return false;
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_GB_DMG, gb, sizeof(struct gb_s), cart_ram,
                                CART_RAM_SIZE);
}

bool PeanutEmu::loadState(const char* romFilename, int slot) {
  if (!gb || !cart_ram || !romFilename) return false;
  struct gb_s* tempGb = (struct gb_s*)heap_caps_malloc(sizeof(struct gb_s), MALLOC_CAP_SPIRAM);
  uint8_t* tempRam = (uint8_t*)heap_caps_malloc(CART_RAM_SIZE, MALLOC_CAP_SPIRAM);
  if (!tempGb || !tempRam) {
    if (tempGb) heap_caps_free(tempGb);
    if (tempRam) heap_caps_free(tempRam);
    return false;
  }
  size_t gbSize = 0, ramSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_GB_DMG, tempGb, sizeof(struct gb_s), &gbSize,
                                   tempRam, CART_RAM_SIZE, &ramSize);
  if (ok && gbSize == sizeof(struct gb_s) && ramSize == CART_RAM_SIZE) {
    memcpy(cart_ram, tempRam, CART_RAM_SIZE);
    tempGb->gb_rom_read = gb_rom_read;
    tempGb->gb_cart_ram_read = gb_cart_ram_read;
    tempGb->gb_cart_ram_write = gb_cart_ram_write;
    tempGb->gb_error = gb_error;
    tempGb->display.lcd_draw_line = lcd_draw_line;
    memcpy(gb, tempGb, sizeof(struct gb_s));
    heap_caps_free(tempRam);
    heap_caps_free(tempGb);
    return true;
  }
  heap_caps_free(tempRam);
  heap_caps_free(tempGb);
  return false;
}
