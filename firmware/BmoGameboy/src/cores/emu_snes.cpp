#pragma GCC optimize("O3,unroll-loops")
#include "emu_snes.h"
#include "../hal/config.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include <Arduino.h>

#if BMO_EXPERIMENTAL_CORES
#include "../hal/buttons.h"
#include "../hal/panel_color.h"
#include "../hal/psram_alloc.h"
#include "../storage/battery_save.h"
#include "../storage/save_manager.h"
#include <string.h>

extern "C" {
#include "../third_party/snes9x/snes9x.h"
#include "../third_party/snes9x/src/dsp.h"
void S9xSetLineCallback(S9xLineCallback cb);
}

namespace {
constexpr size_t kSnesMaxSramBytes = 0x20000U;
uint16_t* s_panelFb = nullptr; // 256 x 224 panel BGR565 in PSRAM
size_t s_sramSaveSize = 0;
bool s_running = false;
uint32_t s_apuRand = 0xA55A1234U;

void snesLineCallback(uint32_t y, const uint16_t* pixels, uint32_t width) {
  if (!s_panelFb || !pixels || y >= 224U || width == 0) return;
  uint16_t* dst = s_panelFb + (size_t)y * 256U;
  if (width > 256U) {
    // Sample down 512 px hi-res pseudo/interlace scanlines cleanly to 256 px.
    for (uint32_t x = 0; x < 256U; ++x) {
      dst[x] = bmo_rgb565_to_panel565(pixels[x * 2U]);
    }
  } else {
    for (uint32_t x = 0; x < width; ++x) {
      dst[x] = bmo_rgb565_to_panel565(pixels[x]);
    }
    for (uint32_t x = width; x < 256U; ++x) {
      dst[x] = 0;
    }
  }
}
} // namespace

extern "C" {

bool S9xInitDisplay(void) {
  GFX.Pitch = SNES_WIDTH * sizeof(uint16_t);
  GFX.Pitch2 = GFX.Pitch;
  GFX.RealPitch = GFX.Pitch;
  GFX.ZPitch = SNES_WIDTH;
  GFX.PPL = SNES_WIDTH;
  GFX.PPLx2 = SNES_WIDTH * 2U;
  GFX.Screen = nullptr;
  GFX.SubScreen = nullptr;
  GFX.ZBuffer = nullptr;
  GFX.SubZBuffer = nullptr;
  GFX.LineRenderMode = true;
  GFX.LinePPL = SNES_WIDTH;
  GFX.LinePitch = SNES_WIDTH * sizeof(uint16_t);
  return true;
}

void S9xDeinitDisplay(void) {}
void S9xExtraUsage(void) {}

void S9xExit(void) {
  s_running = false;
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
  if (port != 0) return 0;
  uint32_t pad = 0;
  if (Buttons::get(Buttons::UP).pressed) pad |= SNES_UP_MASK;
  if (Buttons::get(Buttons::DOWN).pressed) pad |= SNES_DOWN_MASK;
  if (Buttons::get(Buttons::LEFT).pressed) pad |= SNES_LEFT_MASK;
  if (Buttons::get(Buttons::RIGHT).pressed) pad |= SNES_RIGHT_MASK;
  if (Buttons::get(Buttons::A).pressed) pad |= SNES_A_MASK;
  if (Buttons::get(Buttons::B).pressed) pad |= SNES_B_MASK;
  if (Buttons::get(Buttons::X).pressed) pad |= SNES_X_MASK;
  if (Buttons::get(Buttons::Y).pressed) pad |= SNES_Y_MASK;
  if (Buttons::get(Buttons::L).pressed) pad |= SNES_TL_MASK;
  if (Buttons::get(Buttons::R).pressed) pad |= SNES_TR_MASK;
  if (Buttons::get(Buttons::START).pressed) pad |= SNES_START_MASK;
  if (Buttons::get(Buttons::SELECT).pressed) pad |= SNES_SELECT_MASK;
  return pad;
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
  const uint16_t addr = (uint16_t)(0x2140U + (Address & 3));
  if (Memory.FillRAM) {
    Memory.FillRAM[addr] = Byte;
  }
}

uint8_t S9xAPUReadPort(int32_t Address) {
  // Handshake stub answering SPC700 IPL boot signature ($AA / $BB) when audio is off.
  s_apuRand = s_apuRand * 1664525U + 1013904223U;
  const int32_t p = Address & 3;
  if (p < 2) {
    if ((s_apuRand & 2U) != 0U) {
      return (p == 1) ? 0xAAU : 0xBBU;
    }
    if ((s_apuRand & 4U) != 0U) {
      return (p == 1) ? 0xAAU : 0xBBU;
    }
    return (uint8_t)((s_apuRand >> 3) & 0xFFU);
  }
  if ((s_apuRand & 2U) != 0U) {
    return (uint8_t)((s_apuRand >> 3) & 0xFFU);
  }
  const uint16_t addr = (uint16_t)(0x2140U + p);
  return Memory.FillRAM ? Memory.FillRAM[addr] : 0U;
}

void S9xResetAPU(void) {
  if (Memory.FillRAM) {
    Memory.FillRAM[0x2140] = 0xAAU;
    Memory.FillRAM[0x2141] = 0xBBU;
    Memory.FillRAM[0x2142] = 0x00U;
    Memory.FillRAM[0x2143] = 0x00U;
  }
}

} // extern "C"

bool SNESEmu::init(uint8_t* romData, size_t romSize) {
  destroy();
  if (!romData || romSize < 0x8000U) return false;

  s_panelFb = static_cast<uint16_t*>(bmo_psram_calloc(256U * 224U, sizeof(uint16_t)));
  if (!s_panelFb) {
    destroy();
    return false;
  }

  memset(&Settings, 0, sizeof(Settings));
  Settings.CyclesPercentage = 100;
  Settings.H_Max = SNES_CYCLES_PER_SCANLINE;
  Settings.FrameTimePAL = 20000;
  Settings.FrameTimeNTSC = 16667;
  Settings.ControllerOption = SNES_JOYPAD;
  Settings.HBlankStart = (256 * Settings.H_Max) / SNES_HCOUNTER_MAX;
  Settings.SoundPlaybackRate = 0;
  Settings.SoundBufferSize = 0;
  Settings.ThreadSound = false;
  Settings.Mute = true;
  Settings.APUEnabled = false;
  Settings.DisableSoundEcho = false;

  Memory.ROM = romData;
  Memory.ROM_Offset = 0;
  Memory.ROM_AllocSize = (uint32_t)romSize;

  if (!S9xInitDisplay() || !S9xInitMemory() || !S9xInitGFX() || !S9xInitMap() || !S9xInitPpu() ||
      !S9xInitLineBuffers() || !LoadROM(nullptr)) {
    destroy();
    return false;
  }

  s_sramSaveSize = BatterySave::snesBatteryBytes(romData, romSize);
  if (s_sramSaveSize > kSnesMaxSramBytes) s_sramSaveSize = kSnesMaxSramBytes;

  S9xSetLineCallback(snesLineCallback);
  S9xFixColourBrightness();
  S9xReset();

  s_running = true;
  return true;
}

void SNESEmu::update() {
  if (!s_running || !s_panelFb) return;
  IPPU.RenderThisFrame = true;
  S9xMainLoop();
  DisplayEmu::streamSNESFrame(s_panelFb, 256, 224);
}

void SNESEmu::destroy() {
  s_running = false;
  s_sramSaveSize = 0;
  S9xSetLineCallback(nullptr);
  S9xDeinitDSP();
  S9xDeinitGFX();
  S9xDeinitMemory();
  S9xDeinitDisplay();
  if (s_panelFb) {
    bmo_psram_free(s_panelFb);
    s_panelFb = nullptr;
  }
}

bool SNESEmu::isRunning() {
  return s_running;
}

bool SNESEmu::hasActiveBattery() {
  return s_running && Memory.SRAM && s_sramSaveSize > 0;
}

bool SNESEmu::saveBatteryRam(const char* romFilename) {
  if (!s_running || !romFilename || !Memory.SRAM || s_sramSaveSize == 0) return false;
  return SaveManager::saveBatteryRam(romFilename, Memory.SRAM, s_sramSaveSize);
}

bool SNESEmu::loadBatteryRam(const char* romFilename) {
  if (!s_running || !romFilename || !Memory.SRAM || s_sramSaveSize == 0) return false;
  size_t loaded = 0;
  return SaveManager::loadBatteryRam(romFilename, Memory.SRAM, s_sramSaveSize, &loaded);
}

bool SNESEmu::saveState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = S9xBmoSerializeSize();
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!buf) return false;

  size_t outBytes = 0;
  bool ok = S9xBmoSerialize(buf, cap, &outBytes);
  if (ok) {
    ok = SaveManager::saveState(romFilename, slot, SaveManager::CORE_SNES, buf, outBytes,
                                (s_sramSaveSize > 0 && Memory.SRAM) ? Memory.SRAM : nullptr, s_sramSaveSize);
  }
  bmo_psram_free(buf);
  return ok;
}

bool SNESEmu::loadState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = S9xBmoSerializeSize();
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!buf) return false;

  size_t outState = 0;
  size_t outRam = 0;
  bool ok =
      SaveManager::loadState(romFilename, slot, SaveManager::CORE_SNES, buf, cap, &outState,
                             (s_sramSaveSize > 0 && Memory.SRAM) ? Memory.SRAM : nullptr, s_sramSaveSize, &outRam);
  if (ok) {
    ok = S9xBmoUnserialize(buf, outState);
  }
  bmo_psram_free(buf);
  return ok;
}

#else // BMO_EXPERIMENTAL_CORES == 0
bool SNESEmu::init(uint8_t* romData, size_t romSize) {
  (void)romData;
  (void)romSize;
  LOG_WARN_STR("[snes] Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
  DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2200);
  return false;
}

void SNESEmu::update() {}
void SNESEmu::destroy() {}
bool SNESEmu::isRunning() {
  return false;
}
bool SNESEmu::hasActiveBattery() {
  return false;
}
bool SNESEmu::saveBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}
bool SNESEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}
bool SNESEmu::saveState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}
bool SNESEmu::loadState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}

#endif // BMO_EXPERIMENTAL_CORES
