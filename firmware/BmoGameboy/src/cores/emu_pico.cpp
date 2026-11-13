#pragma GCC optimize("O3,unroll-loops")
#include "emu_pico.h"
#include "../hal/config.h"

#include "../third_party/fake08/src/host.h"
#include "../third_party/fake08/src/nibblehelpers.h"

#if BMO_EXPERIMENTAL_CORES

#include <Arduino.h>
#include <string.h>
#include <new>

#include "../hal/buttons.h"
#include "../hal/psram_alloc.h"
#include "../hal/panel_color.h"
#include "../storage/save_manager.h"
#include "../ui/display_emu.h"

#include "../third_party/fake08/src/Audio.h"
#include "../third_party/fake08/src/graphics.h"
#include "../third_party/fake08/src/Input.h"
#include "../third_party/fake08/src/PicoRam.h"
#include "../third_party/fake08/src/fontdata.h"
#include "../third_party/fake08/src/vm.h"

namespace {
constexpr int PICO_W = 128;
constexpr int PICO_H = 128;
constexpr size_t PICO_CARTDATA_SIZE = 256U;
constexpr size_t PICO_LUA_STATE_MAX = 1024U * 1024U;
constexpr size_t PICO_SAVE_STATE_MAX =
    4U + 3U * sizeof(size_t) + PICO_LUA_STATE_MAX + sizeof(PicoRam) + sizeof(musicChannel);

Host* s_host = nullptr;
PicoRam* s_memory = nullptr;
Graphics* s_graphics = nullptr;
Input* s_input = nullptr;
Audio* s_audio = nullptr;
Vm* s_vm = nullptr;
uint16_t* s_panelFb = nullptr;
uint16_t s_panelPalette[144];

uint8_t s_currHeld = 0;
uint8_t s_currDown = 0;
uint8_t s_prevHeld = 0;
std::string s_cartDataKey;
std::string s_cartDataHex;
bool s_running = false;

void initPanelPalette() {
  static const Color kBasePalette[16] = {
      COLOR_00, COLOR_01, COLOR_02, COLOR_03, COLOR_04, COLOR_05, COLOR_06, COLOR_07,
      COLOR_08, COLOR_09, COLOR_10, COLOR_11, COLOR_12, COLOR_13, COLOR_14, COLOR_15,
  };
  static const Color kAltPalette[16] = {
      COLOR_128, COLOR_129, COLOR_130, COLOR_131, COLOR_132, COLOR_133, COLOR_134, COLOR_135,
      COLOR_136, COLOR_137, COLOR_138, COLOR_139, COLOR_140, COLOR_141, COLOR_142, COLOR_143,
  };
  for (int i = 0; i < 16; ++i) {
    s_panelPalette[i] = bmo_rgb888_to_panel565(kBasePalette[i].Red, kBasePalette[i].Green, kBasePalette[i].Blue);
    s_panelPalette[128 + i] = bmo_rgb888_to_panel565(kAltPalette[i].Red, kAltPalette[i].Green, kAltPalette[i].Blue);
  }
  for (int i = 16; i < 128; ++i) {
    s_panelPalette[i] = s_panelPalette[i & 0x0F];
  }
}

void renderPicoToPanel() {
  if (!s_vm || !s_memory || !s_panelFb) return;
  uint8_t* picoFb = s_vm->GetPicoInteralFb();
  uint8_t* screenPaletteMap = s_vm->GetScreenPaletteMap();
  if (!picoFb || !screenPaletteMap) return;

  const uint8_t drawMode = s_memory->drawState.drawMode;
  int scaleX = 1;
  int scaleY = 1;
  if (drawMode == 1 || drawMode == 3) scaleX = 2;
  if (drawMode == 2 || drawMode == 3) scaleY = 2;

  for (int y = 0; y < PICO_H; ++y) {
    for (int x = 0; x < PICO_W; ++x) {
      int sx = x;
      int sy = y;
      switch (drawMode) {
        case 129: sx = (PICO_W - 1) - x; break;
        case 130: sy = (PICO_H - 1) - y; break;
        case 131: sx = (PICO_W - 1) - x; sy = (PICO_H - 1) - y; break;
        case 133: sx = y; sy = (PICO_W - 1) - x; break;
        case 134: sx = (PICO_W - 1) - x; sy = (PICO_H - 1) - y; break;
        case 135: sx = (PICO_H - 1) - y; sy = x; break;
        default: break;
      }
      const int picox = sx / scaleX;
      const int picoy = sy / scaleY;
      const uint8_t rawIdx = getPixelNibble(picox, picoy, picoFb);
      const uint8_t palIdx = screenPaletteMap[rawIdx] & 0x8FU;
      s_panelFb[y * PICO_W + x] = s_panelPalette[palIdx];
    }
  }

  DisplayEmu::streamPicoFrame(s_panelFb);
}
}  // namespace

Host::Host(int windowWidth, int windowHeight)
    : currKDown(0), currKHeld(0), _logFilePrefix("bmo"), _cartDirectory("/") {
  (void)windowWidth;
  (void)windowHeight;
  setUpPaletteColors();
}

void Host::loadSettingsIni() {}
void Host::saveSettingsIni() {}
std::string Host::getCartDataFile(std::string cartDataKey) { return cartDataKey; }

void Host::setUpPaletteColors() {
  static const Color kBase[16] = {
      COLOR_00, COLOR_01, COLOR_02, COLOR_03, COLOR_04, COLOR_05, COLOR_06, COLOR_07,
      COLOR_08, COLOR_09, COLOR_10, COLOR_11, COLOR_12, COLOR_13, COLOR_14, COLOR_15,
  };
  static const Color kAlt[16] = {
      COLOR_128, COLOR_129, COLOR_130, COLOR_131, COLOR_132, COLOR_133, COLOR_134, COLOR_135,
      COLOR_136, COLOR_137, COLOR_138, COLOR_139, COLOR_140, COLOR_141, COLOR_142, COLOR_143,
  };
  for (int i = 0; i < 16; ++i) {
    _paletteColors[i] = kBase[i];
    _paletteColors[128 + i] = kAlt[i];
  }
  for (int i = 16; i < 128; ++i) {
    _paletteColors[i] = kBase[i & 0x0F];
  }
}

void Host::oneTimeSetup(Audio* audio) {
  (void)audio;
  setUpPaletteColors();
}

void Host::unpackCarts() {}
void Host::setTargetFps(int targetFps) { (void)targetFps; }
bool Host::shouldRunMainLoop() { return s_running; }

InputState_t Host::scanInput() {
  InputState_t st;
  st.KDown = s_currDown;
  st.KHeld = s_currHeld;
  st.mouseX = 0;
  st.mouseY = 0;
  st.mouseBtnState = 0;
  st.KBdown = false;
  st.KBkey = "";
  return st;
}

bool Host::shouldQuit() { return !s_running; }
void Host::changeStretch() {}
void Host::forceStretch(StretchOption newStretch) { stretch = newStretch; }
void Host::waitForTargetFps() {}
void Host::drawFrame(uint8_t* picoFb, uint8_t* screenPaletteMap, uint8_t drawMode) {
  (void)picoFb;
  (void)screenPaletteMap;
  (void)drawMode;
}
bool Host::shouldFillAudioBuff() { return false; }
void* Host::getAudioBufferPointer() { return nullptr; }
size_t Host::getAudioBufferSize() { return 0; }
void Host::playFilledAudioBuffer() {}
void Host::oneTimeCleanup() {}
double Host::deltaTMs() { return 16.6667; }
std::vector<std::string> Host::listcarts() { return {}; }
void Host::overrideLogFilePrefix(const char* newPrefix) {
  if (newPrefix) _logFilePrefix = newPrefix;
}
const char* Host::logFilePrefix() { return _logFilePrefix.c_str(); }
std::string Host::customBiosLua() { return _customBiosLua; }

std::string Host::getCartDataFileContents(std::string cartDataKey) {
  if (cartDataKey == s_cartDataKey) return s_cartDataHex;
  return "";
}

void Host::saveCartData(std::string cartDataKey, std::string contents) {
  s_cartDataKey = cartDataKey;
  s_cartDataHex = contents;
}

size_t Host::getFileContents(std::string fileName, char* buffer) {
  (void)fileName;
  (void)buffer;
  return 0;
}

void Host::writeBufferToFile(std::string cartDataKey, char* buffer, size_t length) {
  (void)cartDataKey;
  (void)buffer;
  (void)length;
}

std::string Host::getCartDirectory() { return _cartDirectory; }
void Host::setCartDirectory(std::string cartDirectory) { _cartDirectory = cartDirectory; }
std::vector<std::string> Host::listdirs() { return {}; }

int Host::getSetting(std::string sname) {
  if (sname == "stretch") return (int)stretch;
  if (sname == "kbmode") return (int)kbmode;
  if (sname == "resizekey") return (int)resizekey;
  if (sname == "menustyle") return (int)menustyle;
  if (sname == "bgcolor") return (int)bgcolor;
  return 0;
}

void Host::setSetting(std::string sname, int sdata) {
  if (sname == "stretch") stretch = (StretchOption)sdata;
  else if (sname == "kbmode") kbmode = (KeyboardOption)sdata;
  else if (sname == "resizekey") resizekey = (ResizekeyOption)sdata;
  else if (sname == "menustyle") menustyle = (MenuStyleOption)sdata;
  else if (sname == "bgcolor") bgcolor = (BgColorOption)sdata;
}

Color* Host::GetPaletteColors() { return _paletteColors; }

void Host::setPlatformParams(int windowWidth, int windowHeight, uint32_t sdlWindowFlags,
                             uint32_t sdlRendererFlags, uint32_t sdlPixelFormat,
                             std::string logFilePrefixStr, std::string customBiosLuaStr,
                             std::string cartDirectoryStr) {
  (void)windowWidth;
  (void)windowHeight;
  (void)sdlWindowFlags;
  (void)sdlRendererFlags;
  (void)sdlPixelFormat;
  _logFilePrefix = logFilePrefixStr;
  _customBiosLua = customBiosLuaStr;
  _cartDirectory = cartDirectoryStr;
}

bool PicoEmu::begin(const uint8_t* romData, size_t romSize) {
  if (!romData || romSize == 0) return false;
  destroy();

  s_currHeld = 0;
  s_currDown = 0;
  s_prevHeld = 0;
  s_cartDataKey.clear();
  s_cartDataHex.clear();

  s_memory = (PicoRam*)bmo_psram_calloc(1, sizeof(PicoRam));
  s_panelFb = (uint16_t*)bmo_psram_calloc(PICO_W * PICO_H, sizeof(uint16_t));
  if (!s_memory || !s_panelFb) {
    destroy();
    return false;
  }
  new (s_memory) PicoRam();

  s_host = new Host(PICO_W, PICO_H);
  s_graphics = new Graphics(get_font_data(), s_memory);
  s_input = new Input(s_memory);
  s_audio = new Audio(s_memory);
  if (!s_host || !s_graphics || !s_input || !s_audio) {
    destroy();
    return false;
  }

  // Pass all dependencies non-null so Vm::~Vm keeps _cleanupDeps = false and never
  // calls operator delete on our PSRAM-allocated s_memory block.
  s_vm = new Vm(s_host, s_memory, s_graphics, s_input, s_audio);
  if (!s_vm) {
    destroy();
    return false;
  }

  s_host->oneTimeSetup(s_audio);
  initPanelPalette();

  s_running = true;
  if (!s_vm->LoadCart(romData, romSize, true)) {
    destroy();
    return false;
  }
  s_vm->vm_run();
  return true;
}

void PicoEmu::updateJoypad() {
  if (!s_running) return;
  uint8_t held = 0;
  if (Buttons::get(Buttons::LEFT).pressed) held |= P8_KEY_LEFT;
  if (Buttons::get(Buttons::RIGHT).pressed) held |= P8_KEY_RIGHT;
  if (Buttons::get(Buttons::UP).pressed) held |= P8_KEY_UP;
  if (Buttons::get(Buttons::DOWN).pressed) held |= P8_KEY_DOWN;
  if (Buttons::get(Buttons::B).pressed) held |= P8_KEY_O;
  if (Buttons::get(Buttons::A).pressed) held |= P8_KEY_X;
  if (Buttons::get(Buttons::START).pressed) held |= P8_KEY_PAUSE;

  s_currDown = held & (uint8_t)(~s_prevHeld);
  s_currHeld = held;
  s_prevHeld = held;
}

void PicoEmu::runFrame() {
  if (!s_running || !s_vm) return;
  s_vm->Step();
  renderPicoToPanel();
}

void PicoEmu::destroy() {
  s_running = false;
  s_currHeld = 0;
  s_currDown = 0;
  s_prevHeld = 0;
  s_cartDataKey.clear();
  s_cartDataHex.clear();

  if (s_vm) {
    delete s_vm;
    s_vm = nullptr;
  }
  if (s_audio) {
    delete s_audio;
    s_audio = nullptr;
  }
  if (s_input) {
    delete s_input;
    s_input = nullptr;
  }
  if (s_graphics) {
    delete s_graphics;
    s_graphics = nullptr;
  }
  if (s_memory) {
    s_memory->~PicoRam();
    bmo_psram_free(s_memory);
    s_memory = nullptr;
  }
  if (s_host) {
    delete s_host;
    s_host = nullptr;
  }
  if (s_panelFb) {
    bmo_psram_free(s_panelFb);
    s_panelFb = nullptr;
  }
}

bool PicoEmu::saveBatteryRam(const char* romFilename) {
  if (!s_running || !s_memory || !romFilename) return false;
  return SaveManager::saveBatteryRam(romFilename, &s_memory->data[0x5e00], PICO_CARTDATA_SIZE);
}

bool PicoEmu::loadBatteryRam(const char* romFilename) {
  if (!s_running || !s_memory || !s_vm || !romFilename) return false;
  size_t loaded = 0;
  if (!SaveManager::loadBatteryRam(romFilename, &s_memory->data[0x5e00],
                                   PICO_CARTDATA_SIZE, &loaded) || loaded == 0) {
    return false;
  }
  // Keep in-memory hex cache synchronized if the cart invokes cartdata() later.
  s_cartDataHex = s_vm->getSerializedCartData();
  return true;
}

bool PicoEmu::saveState(const char* romFilename, int slot) {
  if (!s_running || !s_vm || !s_memory || !s_audio || !romFilename) return false;

  char* luaBuf = (char*)bmo_psram_malloc(PICO_LUA_STATE_MAX);
  uint8_t* stateBuf = (uint8_t*)bmo_psram_malloc(PICO_SAVE_STATE_MAX);
  if (!luaBuf || !stateBuf) {
    if (luaBuf) bmo_psram_free(luaBuf);
    if (stateBuf) bmo_psram_free(stateBuf);
    return false;
  }

  const size_t luaLen = s_vm->serializeLuaState(luaBuf);
  bool ok = false;
  if (luaLen > 0 && luaLen <= PICO_LUA_STATE_MAX) {
    const size_t ramSize = sizeof(PicoRam);
    const size_t musicSize = sizeof(musicChannel);
    const size_t totalSize = 4U + sizeof(size_t) + luaLen +
                             sizeof(size_t) + ramSize +
                             sizeof(size_t) + musicSize;
    if (totalSize <= PICO_SAVE_STATE_MAX) {
      size_t off = 0;
      stateBuf[off++] = 'f';
      stateBuf[off++] = '8';
      stateBuf[off++] = 0;
      stateBuf[off++] = 1;

      memcpy(stateBuf + off, &luaLen, sizeof(size_t));
      off += sizeof(size_t);
      memcpy(stateBuf + off, luaBuf, luaLen);
      off += luaLen;

      memcpy(stateBuf + off, &ramSize, sizeof(size_t));
      off += sizeof(size_t);
      memcpy(stateBuf + off, s_memory->data, ramSize);
      off += ramSize;

      memcpy(stateBuf + off, &musicSize, sizeof(size_t));
      off += sizeof(size_t);
      memcpy(stateBuf + off, &s_audio->getAudioState()->_musicChannel, musicSize);
      off += musicSize;

      ok = SaveManager::saveState(romFilename, slot, SaveManager::CORE_PICO8,
                                  stateBuf, off, nullptr, 0);
    }
  }

  bmo_psram_free(stateBuf);
  bmo_psram_free(luaBuf);
  return ok;
}

bool PicoEmu::loadState(const char* romFilename, int slot) {
  if (!s_running || !s_vm || !s_memory || !s_audio || !romFilename) return false;

  uint8_t* stateBuf = (uint8_t*)bmo_psram_malloc(PICO_SAVE_STATE_MAX);
  if (!stateBuf) return false;

  size_t outSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_PICO8,
                                   stateBuf, PICO_SAVE_STATE_MAX, &outSize,
                                   nullptr, 0, nullptr);
  if (ok && outSize >= 4U + sizeof(size_t) && stateBuf[0] == 'f' && stateBuf[1] == '8') {
    size_t off = 4U;
    size_t luaLen = 0;
    memcpy(&luaLen, stateBuf + off, sizeof(size_t));
    off += sizeof(size_t);

    if (luaLen > 0 && luaLen <= PICO_LUA_STATE_MAX && off + luaLen + sizeof(size_t) <= outSize) {
      s_vm->deserializeLuaState(reinterpret_cast<const char*>(stateBuf + off), luaLen);
      off += luaLen;

      size_t picoRamSize = 0;
      memcpy(&picoRamSize, stateBuf + off, sizeof(size_t));
      off += sizeof(size_t);
      if (picoRamSize == sizeof(PicoRam) && off + picoRamSize + sizeof(size_t) <= outSize) {
        memcpy(s_memory->data, stateBuf + off, picoRamSize);
        off += picoRamSize;

        size_t musicSize = 0;
        memcpy(&musicSize, stateBuf + off, sizeof(size_t));
        off += sizeof(size_t);
        if (musicSize == sizeof(musicChannel) && off + musicSize <= outSize) {
          memcpy(&s_audio->getAudioState()->_musicChannel, stateBuf + off, musicSize);
          ok = true;
        } else {
          ok = false;
        }
      } else {
        ok = false;
      }
    } else {
      ok = false;
    }
  } else {
    ok = false;
  }

  bmo_psram_free(stateBuf);
  return ok;
}

#else  // BMO_EXPERIMENTAL_CORES == 0

Host::Host(int windowWidth, int windowHeight) : currKDown(0), currKHeld(0) {
  (void)windowWidth;
  (void)windowHeight;
}
void Host::loadSettingsIni() {}
void Host::saveSettingsIni() {}
std::string Host::getCartDataFile(std::string cartDataKey) { return cartDataKey; }
void Host::setUpPaletteColors() {}
void Host::oneTimeSetup(Audio* audio) { (void)audio; }
void Host::unpackCarts() {}
void Host::setTargetFps(int targetFps) { (void)targetFps; }
bool Host::shouldRunMainLoop() { return false; }
InputState_t Host::scanInput() { return InputState_t{}; }
bool Host::shouldQuit() { return true; }
void Host::changeStretch() {}
void Host::forceStretch(StretchOption newStretch) { (void)newStretch; }
void Host::waitForTargetFps() {}
void Host::drawFrame(uint8_t* picoFb, uint8_t* screenPaletteMap, uint8_t drawMode) {
  (void)picoFb; (void)screenPaletteMap; (void)drawMode;
}
bool Host::shouldFillAudioBuff() { return false; }
void* Host::getAudioBufferPointer() { return nullptr; }
size_t Host::getAudioBufferSize() { return 0; }
void Host::playFilledAudioBuffer() {}
void Host::oneTimeCleanup() {}
double Host::deltaTMs() { return 0.0; }
std::vector<std::string> Host::listcarts() { return {}; }
void Host::overrideLogFilePrefix(const char* newPrefix) { (void)newPrefix; }
const char* Host::logFilePrefix() { return ""; }
std::string Host::customBiosLua() { return ""; }
std::string Host::getCartDataFileContents(std::string cartDataKey) { (void)cartDataKey; return ""; }
void Host::saveCartData(std::string cartDataKey, std::string contents) { (void)cartDataKey; (void)contents; }
size_t Host::getFileContents(std::string fileName, char* buffer) { (void)fileName; (void)buffer; return 0; }
void Host::writeBufferToFile(std::string cartDataKey, char* buffer, size_t length) {
  (void)cartDataKey; (void)buffer; (void)length;
}
std::string Host::getCartDirectory() { return ""; }
void Host::setCartDirectory(std::string cartDirectory) { (void)cartDirectory; }
std::vector<std::string> Host::listdirs() { return {}; }
int Host::getSetting(std::string sname) { (void)sname; return 0; }
void Host::setSetting(std::string sname, int sdata) { (void)sname; (void)sdata; }
Color* Host::GetPaletteColors() { return _paletteColors; }
void Host::setPlatformParams(int, int, uint32_t, uint32_t, uint32_t, std::string, std::string, std::string) {}

bool PicoEmu::begin(const uint8_t* romData, size_t romSize) {
  (void)romData;
  (void)romSize;
  return false;
}

void PicoEmu::updateJoypad() {}
void PicoEmu::runFrame() {}
void PicoEmu::destroy() {}

bool PicoEmu::saveBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool PicoEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool PicoEmu::saveState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}

bool PicoEmu::loadState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}

#endif
