#pragma GCC optimize ("O3,unroll-loops")
#include "emu_genesis.h"
#include "../hal/config.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include <Arduino.h>

#if BMO_EXPERIMENTAL_CORES
#include "../hal/buttons.h"
#include "../hal/panel_color.h"
#include "../hal/psram_alloc.h"
#include "../storage/save_manager.h"
#include <string.h>

extern "C" {
  #include "../third_party/gwenesis/bus/gwenesis_bus.h"
  #include "../third_party/gwenesis/cpus/M68K/m68k.h"
  #include "../third_party/gwenesis/io/gwenesis_io.h"
  #include "../third_party/gwenesis/vdp/gwenesis_vdp.h"
  #include "../third_party/gwenesis/sound/ym2612.h"
  #include "../third_party/gwenesis/sound/gwenesis_sn76489.h"
  #include "../third_party/gwenesis/sound/z80inst.h"
  #include "../third_party/gwenesis/savestate/gwenesis_savestate.h"

  void load_cartridge(unsigned char* buffer, size_t size);
  void power_on(void);
  void reset_emulation(void);
  void gwenesis_SN76489_Init(int PSGClockValue, int SamplingRate, int freq_divisor);
}

// Globals required by Gwenesis core modules
extern "C" {
  int scan_line = 0;
  int system_clock = 0;
  int frame_counter = 0;
  uint16_t v_counter = 0;
  int16_t* gwenesis_ym2612_buffer = nullptr;
  int16_t* gwenesis_sn76489_buffer = nullptr;
}

namespace {
  constexpr int kGenWidth = 320;
  constexpr int kGenHeight = 224;
  constexpr size_t kGenMaxArchiveBytes = 256U * 1024U;
  uint16_t* s_panelFb = nullptr; // 320 x 224 panel BGR565 in PSRAM
  bool s_running = false;
  volatile bool s_sramDirty = false;
} // namespace

extern "C" void genesis_save_mark_dirty_c(void) {
  s_sramDirty = true;
}

extern "C" void GWENESIS_PUSH_SCANLINE(int line, const uint16_t* src16, int w) {
  if (!s_panelFb || !src16 || line < 0 || line >= kGenHeight || w <= 0) return;
  uint16_t* dst = s_panelFb + (size_t)line * kGenWidth;
  if (w >= kGenWidth) {
    for (int x = 0; x < kGenWidth; ++x) {
      dst[x] = bmo_rgb565_to_panel565(src16[x]);
    }
  } else {
    // Center H32 (256 px) lines inside 320 px display framebuffer.
    const int leftPad = (kGenWidth - w) / 2;
    for (int x = 0; x < leftPad; ++x) dst[x] = 0;
    for (int x = 0; x < w; ++x) {
      dst[leftPad + x] = bmo_rgb565_to_panel565(src16[x]);
    }
    for (int x = leftPad + w; x < kGenWidth; ++x) dst[x] = 0;
  }
}

extern "C" void gwenesis_io_get_buttons(void) {
  // Gwenesis bit positions (gwenesis_io.c): 0:U 1:D 2:L 3:R 4:C 5:B 6:A 7:Start
  for (int b = 0; b < 8; ++b) {
    gwenesis_io_pad_release_button(0, b);
  }
  if (Buttons::get(Buttons::UP).pressed)     gwenesis_io_pad_press_button(0, 0);
  if (Buttons::get(Buttons::DOWN).pressed)   gwenesis_io_pad_press_button(0, 1);
  if (Buttons::get(Buttons::LEFT).pressed)   gwenesis_io_pad_press_button(0, 2);
  if (Buttons::get(Buttons::RIGHT).pressed)  gwenesis_io_pad_press_button(0, 3);
  // Map SELECT/X -> C so 3-button Genesis titles are playable on the wired A/B/START/SELECT board.
  if (Buttons::get(Buttons::X).pressed || Buttons::get(Buttons::SELECT).pressed) {
    gwenesis_io_pad_press_button(0, 4);
  }
  if (Buttons::get(Buttons::B).pressed)      gwenesis_io_pad_press_button(0, 5);
  if (Buttons::get(Buttons::A).pressed)      gwenesis_io_pad_press_button(0, 6);
  if (Buttons::get(Buttons::START).pressed)  gwenesis_io_pad_press_button(0, 7);
}

bool GenesisEmu::init(uint8_t* romData, size_t romSize) {
  destroy();
  if (!romData || romSize < 0x200U) return false;

  s_panelFb = static_cast<uint16_t*>(bmo_psram_calloc((size_t)kGenWidth * kGenHeight, sizeof(uint16_t)));
  VRAM      = static_cast<unsigned char*>(bmo_psram_calloc(VRAM_MAX_SIZE, 1));
  M68K_RAM  = static_cast<unsigned char*>(bmo_psram_calloc(MAX_RAM_SIZE, 1));
  ZRAM      = static_cast<unsigned char*>(bmo_psram_calloc(MAX_Z80_RAM_SIZE, 1));
  if (!s_panelFb || !VRAM || !M68K_RAM || !ZRAM) {
    destroy();
    return false;
  }

  gwenesis_vdp_allocate_buffers();
  gwenesis_ym2612_tables_alloc();
  load_cartridge(static_cast<unsigned char*>(romData), romSize);
  gwenesis_init_sram(romData, static_cast<uint32_t>(romSize));
  power_on();
  reset_emulation();

  scan_line = 0;
  system_clock = 0;
  frame_counter = 0;
  v_counter = 0;

  s_running = true;
  return true;
}

void GenesisEmu::update() {
  if (!s_running || !s_panelFb) return;

  gwenesis_vdp_render_config();
  const int visibleH = REG1_PAL ? 240 : 224;
  const int linesPerFrame = REG1_PAL ? 313 : 262;
  int hintCounter = gwenesis_vdp_regs[10];
  int cpuDeadline = 0;

  for (scan_line = 0; scan_line < linesPerFrame; ++scan_line) {
    cpuDeadline += VDP_CYCLES_PER_LINE;
    system_clock = cpuDeadline;
    v_counter = static_cast<uint16_t>(scan_line);

    m68k_run(static_cast<unsigned int>(cpuDeadline));
    z80_run(cpuDeadline);

    if (scan_line < visibleH && scan_line < kGenHeight) {
      gwenesis_vdp_render_line(scan_line);
    }

    if (scan_line == 0 || scan_line > visibleH) {
      hintCounter = REG10_LINE_COUNTER;
    }

    if (--hintCounter < 0) {
      if (REG0_LINE_INTERRUPT != 0 && scan_line <= visibleH) {
        hint_pending = 1;
        if ((gwenesis_vdp_status & STATUS_VIRQPENDING) == 0) {
          m68k_update_irq(4);
        }
      }
      hintCounter = REG10_LINE_COUNTER;
    }

    if (scan_line == visibleH) {
      if (REG1_VBLANK_INTERRUPT != 0) {
        gwenesis_vdp_status |= STATUS_VIRQPENDING;
        m68k_set_irq(6);
      }
      z80_irq_line(1);
    } else if (scan_line == visibleH + 1) {
      z80_irq_line(0);
    }
  }

  m68k.cycles -= cpuDeadline;
  ++frame_counter;
  DisplayEmu::streamGenesisFrame(s_panelFb, kGenWidth, kGenHeight);
}

void GenesisEmu::destroy() {
  s_running = false;
  saveGwenesisStateResetArchive();
  gwenesis_vdp_free_buffers();
  gwenesis_ym2612_free();
  m68k_free();
  gwenesis_bus_free();
  if (s_panelFb) {
    bmo_psram_free(s_panelFb);
    s_panelFb = nullptr;
  }
}

bool GenesisEmu::isRunning() {
  return s_running;
}

static size_t genesisSramBytes() {
  if (!SRAM_ENABLED || !SRAM || SRAM_END < SRAM_START) return 0;
  size_t sz = static_cast<size_t>(SRAM_END - SRAM_START + 1U);
  if (sz > SRAM_SIZE) sz = SRAM_SIZE;
  return sz;
}

bool GenesisEmu::saveBatteryRam(const char* romFilename) {
  const size_t sz = genesisSramBytes();
  if (!s_running || !romFilename || sz == 0) return false;
  return SaveManager::saveBatteryRam(romFilename, SRAM, sz);
}

bool GenesisEmu::loadBatteryRam(const char* romFilename) {
  const size_t sz = genesisSramBytes();
  if (!s_running || !romFilename || sz == 0) return false;
  size_t loaded = 0;
  return SaveManager::loadBatteryRam(romFilename, SRAM, sz, &loaded);
}

bool GenesisEmu::saveState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(kGenMaxArchiveBytes));
  if (!buf) return false;

  size_t outBytes = 0;
  bool ok = saveGwenesisStateSaveToBuffer(buf, kGenMaxArchiveBytes, &outBytes);
  if (ok) {
    const size_t sramSz = genesisSramBytes();
    ok = SaveManager::saveState(
      romFilename, slot, SaveManager::CORE_GENESIS,
      buf, outBytes,
      (sramSz > 0) ? SRAM : nullptr, sramSz
    );
  }
  saveGwenesisStateResetArchive();
  bmo_psram_free(buf);
  return ok;
}

bool GenesisEmu::loadState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(kGenMaxArchiveBytes));
  if (!buf) return false;

  size_t outState = 0;
  size_t outRam = 0;
  const size_t sramSz = genesisSramBytes();
  bool ok = SaveManager::loadState(
    romFilename, slot, SaveManager::CORE_GENESIS,
    buf, kGenMaxArchiveBytes, &outState,
    (sramSz > 0) ? SRAM : nullptr, sramSz, &outRam
  );
  if (ok) {
    ok = saveGwenesisStateLoadFromBuffer(buf, outState);
  }
  saveGwenesisStateResetArchive();
  bmo_psram_free(buf);
  return ok;
}

#else // BMO_EXPERIMENTAL_CORES == 0
bool GenesisEmu::init(uint8_t* romData, size_t romSize) {
  (void)romData;
  (void)romSize;
  LOG_WARN_STR("[genesis] Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
  DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2200);
  return false;
}

void GenesisEmu::update() {}
void GenesisEmu::destroy() {}
bool GenesisEmu::isRunning() { return false; }
bool GenesisEmu::saveBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool GenesisEmu::loadBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool GenesisEmu::saveState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }
bool GenesisEmu::loadState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }

#endif // BMO_EXPERIMENTAL_CORES
