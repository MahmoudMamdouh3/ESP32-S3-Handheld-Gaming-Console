#pragma GCC optimize ("O3,unroll-loops")
#include "emu_ngp.h"
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
  #include "../third_party/race/types.h"
  #include "../third_party/race/retro_compat.h"
  #include "../third_party/race/race-memory.h"
  #include "../third_party/race/cz80.h"
  #include "../third_party/race/tlcs900h.h"
  #include "../third_party/race/cz80_support.h"
  #include "../third_party/race/graphics.h"
  #include "../third_party/race/input.h"
  #include "../third_party/race/flash.h"
  #include "../third_party/race/neopopsound.h"
  #include "../third_party/race/state.h"

  extern int finscan;
  extern void tlcs_execute(int cycles);
}

// Globals expected by RACE submodules
extern "C" {
  int m_bIsActive = 0;
  EMUINFO m_emuInfo = { NGPC, 0, { 0 } };
  struct ngp_screen* race_ngp_screen = nullptr;
  char retro_save_directory[3] = "/";
  int tipo_consola = 0;
  int gfx_hacks = 0;
  volatile unsigned g_frame_ready = 0;
  bool s_interlace_parity = false;
  uint8_t* s_lut_y_render = nullptr;
  unsigned short* drawBuffer = nullptr;

  unsigned char* rasterY = nullptr;
  unsigned char* frame0Pri = nullptr;
  unsigned char* frame1Pri = nullptr;
  unsigned char* color_switch = nullptr;
  unsigned char* scanlineY = nullptr;
  unsigned char* scrollSpriteX = nullptr;
  unsigned char* scrollSpriteY = nullptr;
  unsigned char* sprite_palette_numbers = nullptr;
  unsigned char* sprite_table = nullptr;
  unsigned short* patterns = nullptr;
  unsigned char* oowSelect = nullptr;
  unsigned short* oowTable = nullptr;
  unsigned char* wndTopLeftY = nullptr;
  unsigned char* wndSizeY = nullptr;
  unsigned char* wndSizeX = nullptr;
  unsigned char* bgSelect = nullptr;
  unsigned char* bw_palette_table = nullptr;
  unsigned short* palette_table = nullptr;
  unsigned short* bgTable = nullptr;
  unsigned char* wndTopLeftX = nullptr;
  unsigned char* scrollFrontY = nullptr;
  unsigned char* scrollFrontX = nullptr;
  unsigned short* tile_table_front = nullptr;
  unsigned char* scrollBackY = nullptr;
  unsigned short* tile_table_back = nullptr;
  unsigned char* scrollBackX = nullptr;
  unsigned char* pattern_table = nullptr;
}

namespace {
  constexpr int kNgpWidth = 160;
  constexpr int kNgpHeight = 152;
  constexpr int kCyclesPerFrame = 6144000 / 60;
  uint16_t* s_panelFb = nullptr; // 160 x 152 panel BGR565 in PSRAM
  uint16_t* s_rawFb = nullptr;   // 160 x 152 RGB565 backing drawBuffer in PSRAM
  unsigned char s_dummyScanline = 0;
  bool s_running = false;

  void mapVdpTables() {
    sprite_table           = const_cast<unsigned char*>(get_address(0x00008800));
    pattern_table          = const_cast<unsigned char*>(get_address(0x0000A000));
    patterns               = reinterpret_cast<unsigned short*>(pattern_table);
    tile_table_front       = const_cast<unsigned short*>(reinterpret_cast<const unsigned short*>(get_address(0x00009000)));
    tile_table_back        = const_cast<unsigned short*>(reinterpret_cast<const unsigned short*>(get_address(0x00009800)));
    palette_table          = const_cast<unsigned short*>(reinterpret_cast<const unsigned short*>(get_address(0x00008200)));
    bw_palette_table       = const_cast<unsigned char*>(get_address(0x00008100));
    sprite_palette_numbers = const_cast<unsigned char*>(get_address(0x00008C00));

    scanlineY              = const_cast<unsigned char*>(get_address(0x00008009));
    frame0Pri              = const_cast<unsigned char*>(get_address(0x00008000));
    frame1Pri              = const_cast<unsigned char*>(get_address(0x00008030));

    wndTopLeftX            = const_cast<unsigned char*>(get_address(0x00008002));
    wndTopLeftY            = const_cast<unsigned char*>(get_address(0x00008003));
    wndSizeX               = const_cast<unsigned char*>(get_address(0x00008004));
    wndSizeY               = const_cast<unsigned char*>(get_address(0x00008005));

    scrollSpriteX          = const_cast<unsigned char*>(get_address(0x00008020));
    scrollSpriteY          = const_cast<unsigned char*>(get_address(0x00008021));
    scrollFrontX           = const_cast<unsigned char*>(get_address(0x00008032));
    scrollFrontY           = const_cast<unsigned char*>(get_address(0x00008033));
    scrollBackX            = const_cast<unsigned char*>(get_address(0x00008034));
    scrollBackY            = const_cast<unsigned char*>(get_address(0x00008035));

    bgSelect               = const_cast<unsigned char*>(get_address(0x00008118));
    bgTable                = const_cast<unsigned short*>(reinterpret_cast<const unsigned short*>(get_address(0x000083E0)));
    oowSelect              = const_cast<unsigned char*>(get_address(0x00008012));
    oowTable               = const_cast<unsigned short*>(reinterpret_cast<const unsigned short*>(get_address(0x000083F0)));

    color_switch           = const_cast<unsigned char*>(get_address(0x00006F91));

    if (!scanlineY) scanlineY = &s_dummyScanline;
    rasterY = scanlineY;
  }

  void pollInput() {
    uint8_t state = 0;
    if (Buttons::get(Buttons::UP).pressed)     state |= (1U << 0);
    if (Buttons::get(Buttons::DOWN).pressed)   state |= (1U << 1);
    if (Buttons::get(Buttons::LEFT).pressed)   state |= (1U << 2);
    if (Buttons::get(Buttons::RIGHT).pressed)  state |= (1U << 3);
    if (Buttons::get(Buttons::A).pressed)      state |= (1U << 4);
    if (Buttons::get(Buttons::B).pressed)      state |= (1U << 5);
    if (Buttons::get(Buttons::START).pressed || Buttons::get(Buttons::SELECT).pressed) {
      state |= (1U << 6); // OPTION
    }
    ngpInputState = state;
  }
} // namespace

bool NGPEmu::init(uint8_t* romData, size_t romSize, bool isColor) {
  destroy();
  if (!romData || romSize < 64U) return false;

  s_panelFb = static_cast<uint16_t*>(bmo_psram_calloc((size_t)kNgpWidth * kNgpHeight, sizeof(uint16_t)));
  s_rawFb   = static_cast<uint16_t*>(bmo_psram_calloc((size_t)kNgpWidth * kNgpHeight, sizeof(uint16_t)));
  s_lut_y_render = static_cast<uint8_t*>(bmo_psram_calloc(256U, sizeof(uint8_t)));
  if (!s_panelFb || !s_rawFb || !s_lut_y_render) {
    destroy();
    return false;
  }
  memset(s_lut_y_render, 1, kNgpHeight);
  drawBuffer = s_rawFb;

  if (!ngp_mem_set_rom(romData, romSize)) {
    destroy();
    return false;
  }
  setFlashSize(static_cast<unsigned int>(romSize));
  if (!ngp_mem_alloc_init(0x18000U, 0x10000U)) {
    destroy();
    return false;
  }

  m_emuInfo.machine = isColor ? NGPC : NGP;
  m_emuInfo.romSize = static_cast<int>(romSize);
  tipo_consola      = isColor ? 0 : 1;

  Cz80_allocate_flag_tables();
  ngp_mem_init();
  flashStartup();
  mapVdpTables();

  if (bgTable)  bgTable[0] = 0xFFFF;
  if (bgSelect) *bgSelect |= 0x80U;

  tlcs_init();
  tlcs_reinit();
  Z80_Init();
  Z80_Reset();

  // Configure post-boot OS flags cleanly.
  tlcsMemWriteB(0x00006F91, isColor ? tlcsMemReadB(0x00200023) : 0x00);
  tlcsMemWriteB(0x00006F87, 0x01); // English language
  tlcsMemWriteB(0x00004000, tlcsMemReadB(0x00004000) | 0xC0U);
  tlcsMemWriteB(0x00006F84, 0x40);
  tlcsMemWriteB(0x00006F85, 0x00);
  tlcsMemWriteB(0x00006F86, 0x00);

  if (!graphics_init()) {
    destroy();
    return false;
  }

  if (wndTopLeftX && wndTopLeftY && wndSizeX && wndSizeY) {
    if (*wndSizeX == 0 || *wndSizeY == 0) {
      *wndTopLeftX = 0; *wndTopLeftY = 0;
      *wndSizeX = kNgpWidth; *wndSizeY = kNgpHeight;
    }
  }
  if (frame0Pri) *frame0Pri |= 0xC0U;
  finscan = (mainrom[0x000020] == 0x65 || mainrom[0x000020] == 0x93) ? 199 : 198;

  const unsigned short romId = tlcsMemReadW(0x00200020);
  if (romId == 0x0059 || romId == 0x0061) {
    tlcsMemWriteB(0x0020001F, 0xFF);
  }

  m_bIsActive = 1;
  s_running = true;
  return true;
}

void NGPEmu::update() {
  if (!s_running || !s_panelFb || !s_rawFb) return;
  pollInput();
  tlcs_execute(kCyclesPerFrame);

  const size_t total = (size_t)kNgpWidth * kNgpHeight;
  for (size_t i = 0; i < total; ++i) {
    s_panelFb[i] = bmo_rgb565_to_panel565(s_rawFb[i]);
  }
  DisplayEmu::streamNGPFrame(s_panelFb, kNgpWidth, kNgpHeight);
}

void NGPEmu::destroy() {
  s_running = false;
  m_bIsActive = 0;
  flashShutdown();
  tlcs_free_tables();
  graphics_free();
  Cz80_free_flag_tables();
  neopopsound_free();
  ngp_mem_free();
  drawBuffer = nullptr;
  if (s_lut_y_render) { bmo_psram_free(s_lut_y_render); s_lut_y_render = nullptr; }
  if (s_rawFb)        { bmo_psram_free(s_rawFb);        s_rawFb = nullptr; }
  if (s_panelFb)      { bmo_psram_free(s_panelFb);      s_panelFb = nullptr; }
}

bool NGPEmu::isRunning() {
  return s_running;
}

bool NGPEmu::saveBatteryRam(const char* romFilename) {
  const size_t sz = flashGetSaveSize();
  if (!s_running || !romFilename || !ngpSaveBuf || !ngpSaveBufActive || sz == 0) return false;
  return SaveManager::saveBatteryRam(romFilename, ngpSaveBuf, sz);
}

bool NGPEmu::loadBatteryRam(const char* romFilename) {
  const size_t sz = flashGetSaveSize();
  if (!s_running || !romFilename || !ngpSaveBuf || !ngpSaveBufActive || sz == 0) return false;
  size_t loaded = 0;
  return SaveManager::loadBatteryRam(romFilename, ngpSaveBuf, sz, &loaded);
}

bool NGPEmu::saveState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = static_cast<size_t>(state_get_size());
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!buf) return false;

  bool ok = (state_store_mem(buf) != 0);
  if (ok) {
    const size_t flashSz = (ngpSaveBuf && ngpSaveBufActive) ? flashGetSaveSize() : 0;
    ok = SaveManager::saveState(
      romFilename, slot, SaveManager::CORE_NGP,
      buf, cap,
      (flashSz > 0) ? ngpSaveBuf : nullptr, flashSz
    );
  }
  bmo_psram_free(buf);
  return ok;
}

bool NGPEmu::loadState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = static_cast<size_t>(state_get_size());
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!buf) return false;

  size_t outState = 0;
  size_t outRam = 0;
  const size_t flashSz = (ngpSaveBuf && ngpSaveBufActive) ? flashGetSaveSize() : 0;
  bool ok = SaveManager::loadState(
    romFilename, slot, SaveManager::CORE_NGP,
    buf, cap, &outState,
    (flashSz > 0) ? ngpSaveBuf : nullptr, flashSz, &outRam
  );
  if (ok) {
    ok = (state_restore_mem(buf) != 0);
    mapVdpTables();
  }
  bmo_psram_free(buf);
  return ok;
}

#else // BMO_EXPERIMENTAL_CORES == 0
bool NGPEmu::init(uint8_t* romData, size_t romSize, bool isColor) {
  (void)isColor;
  (void)romData;
  (void)romSize;
  LOG_WARN_STR("[ngp] Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
  DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2200);
  return false;
}

void NGPEmu::update() {}
void NGPEmu::destroy() {}
bool NGPEmu::isRunning() { return false; }
bool NGPEmu::saveBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool NGPEmu::loadBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool NGPEmu::saveState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }
bool NGPEmu::loadState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }

#endif // BMO_EXPERIMENTAL_CORES
