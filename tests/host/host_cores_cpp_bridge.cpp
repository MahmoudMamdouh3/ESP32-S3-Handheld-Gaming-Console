/* Host execution harness for C++ cores: Stella 2014 (Atari 2600), Handy (Lynx), Fake-08 (PICO-8). */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "stella2014/src/bspf.h"
#include "stella2014/src/Cart.h"
#include "stella2014/src/Console.h"
#include "stella2014/src/MD5.h"
#include "stella2014/src/OSystem.h"
#include "stella2014/src/Props.h"
#include "stella2014/src/PropsSet.h"
#include "stella2014/src/Settings.h"
#include "stella2014/src/TIA.h"
#include "stella2014/src/TIATables.h"

#include "handy/handy.h"

#include "fake08/src/Audio.h"
#include "fake08/src/graphics.h"
#include "fake08/src/Input.h"
#include "fake08/src/PicoRam.h"
#include "fake08/src/fontdata.h"
#include "fake08/src/host.h"
#include "fake08/src/vm.h"

Host::Host(int, int)
    : currKDown(0), currKHeld(0), _logFilePrefix("bmo"), _cartDirectory("/") {
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
void Host::oneTimeSetup(Audio*) { setUpPaletteColors(); }
void Host::unpackCarts() {}
void Host::setTargetFps(int) {}
bool Host::shouldRunMainLoop() { return true; }
InputState_t Host::scanInput() {
  InputState_t st;
  st.KDown = 0;
  st.KHeld = 0;
  st.mouseX = 0;
  st.mouseY = 0;
  st.mouseBtnState = 0;
  st.KBdown = false;
  st.KBkey = "";
  return st;
}
bool Host::shouldQuit() { return false; }
void Host::changeStretch() {}
void Host::forceStretch(StretchOption s) { stretch = s; }
void Host::waitForTargetFps() {}
void Host::drawFrame(uint8_t*, uint8_t*, uint8_t) {}
bool Host::shouldFillAudioBuff() { return false; }
void* Host::getAudioBufferPointer() { return nullptr; }
size_t Host::getAudioBufferSize() { return 0; }
void Host::playFilledAudioBuffer() {}
void Host::oneTimeCleanup() {}
double Host::deltaTMs() { return 16.6667; }
std::vector<std::string> Host::listcarts() { return {}; }
void Host::overrideLogFilePrefix(const char* p) { if (p) _logFilePrefix = p; }
const char* Host::logFilePrefix() { return _logFilePrefix.c_str(); }
std::string Host::customBiosLua() { return _customBiosLua; }
std::string Host::getCartDataFileContents(std::string) { return ""; }
void Host::saveCartData(std::string, std::string) {}
size_t Host::getFileContents(std::string, char*) { return 0; }
void Host::writeBufferToFile(std::string, char*, size_t) {}
std::string Host::getCartDirectory() { return _cartDirectory; }
void Host::setCartDirectory(std::string d) { _cartDirectory = d; }
std::vector<std::string> Host::listdirs() { return {}; }
int Host::getSetting(std::string) { return 0; }
void Host::setSetting(std::string, int) {}
Color* Host::GetPaletteColors() { return _paletteColors; }
void Host::setPlatformParams(int, int, uint32_t, uint32_t, uint32_t,
                             std::string logFilePrefixStr, std::string customBiosLuaStr,
                             std::string cartDirectoryStr) {
  _logFilePrefix = logFilePrefixStr;
  _customBiosLua = customBiosLuaStr;
  _cartDirectory = cartDirectoryStr;
}

extern "C" int test_atari_core_cpp(void) {
  std::vector<uint8_t> rom(4096U, 0xEAU);
  rom[0xFFC] = 0x00U;
  rom[0xFFD] = 0xF0U;
  const uint8_t prog[] = {
    0xA9U, 0x88U, 0x85U, 0x80U, /* LDA #$88 ; STA $80 */
    0xA9U, 0x2EU, 0x85U, 0x08U, /* LDA #$2E ; STA COLUPF */
    0xA9U, 0x44U, 0x85U, 0x09U, /* LDA #$44 ; STA COLUBK */
    0xA9U, 0xAAU, 0x85U, 0x0EU, /* LDA #$AA ; STA PF1 */
    0xA9U, 0x02U, 0x85U, 0x02U, /* LDA #$02 ; STA WSYNC */
    0x85U, 0x00U, 0x85U, 0x02U, /* STA VSYNC ; STA WSYNC */
    0x85U, 0x02U, 0x85U, 0x02U,
    0xA9U, 0x00U, 0x85U, 0x00U, /* VSYNC off */
    0x85U, 0x01U, 0xA2U, 0xFAU, /* VBLANK off ; LDX #$FA */
    0x85U, 0x02U, 0xCAU, 0xD0U, 0xFBU, /* scanline loop */
    0x4CU, 0x10U, 0xF0U         /* JMP $F010 */
  };
  memcpy(rom.data(), prog, sizeof(prog));

  if (!TIATables::allocateAllTables()) {
    printf("[HOST_CORE] ATARI_2600 STATUS=FAIL (tables)\n");
    return 1;
  }

  OSystem* osys = new OSystem();
  Settings* settings = new Settings(osys);
  osys->attach(settings);
  settings->setValue("palette", "standard");
  settings->setValue("fastscbios", true);
  settings->setValue("rominfo", false);
  settings->setValue("tiadriven", false);
  settings->setValue("ramrandom", false);
  settings->setValue("joyallow4", false);

  string md5 = MD5(rom.data(), static_cast<uint32_t>(rom.size()));
  Properties props;
  osys->propSet().getMD5(md5, props);
  string dtype = props.get(Cartridge_Type);
  string cartId;
  Cartridge* cart = Cartridge::create(rom.data(), static_cast<uint32_t>(rom.size()), md5,
                                      dtype, cartId, *osys, *settings, "");
  Console* console = cart ? new Console(osys, cart, props) : nullptr;
  if (!console) {
    delete cart;
    delete settings;
    delete osys;
    TIATables::freeAllTables();
    printf("[HOST_CORE] ATARI_2600 STATUS=FAIL (console)\n");
    return 1;
  }
  osys->myConsole = console;
  console->initializeVideo();
  console->initializeAudio();

  for (int f = 0; f < 50; ++f) {
    console->tia().update();
  }

  TIA& tia = console->tia();
  const uint8_t* fb = tia.currentFrameBuffer();
  const size_t total = static_cast<size_t>(tia.width()) * tia.height();
  int nz = 0;
  bool seen[256] = {};
  int distinct = 0;
  if (fb) {
    for (size_t i = 0; i < total; ++i) {
      if (fb[i] != 0U) ++nz;
      if (!seen[fb[i]]) {
        seen[fb[i]] = true;
        ++distinct;
      }
    }
  }

  osys->myConsole = nullptr;
  delete console;
  delete settings;
  delete osys;
  TIATables::freeAllTables();

  if (nz < 1000 || distinct < 2) {
    printf("[HOST_CORE] ATARI_2600 STATUS=FAIL non_zero=%d distinct=%d\n", nz, distinct);
    return 1;
  }
  printf("[HOST_CORE] ATARI_2600 STATUS=PASS non_zero=%d distinct=%d\n", nz, distinct);
  return 0;
}

extern "C" int test_lynx_core_cpp(void) {
  /* BLL "BS93" homebrew format (size 0x0101 = 257 bytes so BE/LE length is symmetric,
   * load address 0x0404 symmetric): sets $0080=0x5C, sets Mikie framebuffer $2000 = 0xAA,
   * enables DISPCTL $FD92=0x0D & palette, loops. */
  std::vector<uint8_t> rom(10U + 0x0101U, 0xEAU);
  rom[0] = 0x80U;
  rom[1] = 0x08U;
  rom[2] = 0x04U;
  rom[3] = 0x04U;
  rom[4] = 0x01U;
  rom[5] = 0x01U;
  memcpy(&rom[6], "BS93", 4);

  const uint8_t prog[] = {
    0xA9U, 0x5CU, 0x85U, 0x80U,        /* LDA #$5C ; STA $80 */
    0xA9U, 0x0DU, 0x8DU, 0x92U, 0xFDU, /* LDA #$0D ; STA $FD92 (DISPCTL) */
    0xA9U, 0x00U, 0x8DU, 0x94U, 0xFDU, /* LDA #$00 ; STA $FD94 (DISPADR L) */
    0xA9U, 0x20U, 0x8DU, 0x95U, 0xFDU, /* LDA #$20 ; STA $FD95 (DISPADR H) */
    0xA9U, 0x0FU, 0x8DU, 0xA0U, 0xFDU, /* LDA #$0F ; STA $FDA0 (Green0) */
    0x8DU, 0xA1U, 0xFDU,               /* STA $FDA1 (Green1) */
    0x8DU, 0xB0U, 0xFDU,               /* STA $FDB0 (BlueRed0) */
    0x8DU, 0xB1U, 0xFDU,               /* STA $FDB1 (BlueRed1) */
    0xA2U, 0x00U,                      /* LDX #$00 */
    0xA9U, 0xFFU,                      /* LDA #$FF */
    0x9DU, 0x00U, 0x20U,               /* STA $2000,X */
    0x9DU, 0x00U, 0x21U,               /* STA $2100,X */
    0xE8U, 0xD0U, 0xF7U,               /* INX ; BNE loop */
    0x80U, 0xFEU                       /* BRA -2 */
  };
  memcpy(&rom[10], prog, sizeof(prog));

  uint16_t* raw_fb = static_cast<uint16_t*>(calloc(HANDY_SCREEN_WIDTH * HANDY_SCREEN_HEIGHT, sizeof(uint16_t)));
  if (!raw_fb) {
    printf("[HOST_CORE] ATARI_LYNX STATUS=FAIL (oom)\n");
    return 1;
  }

  gAudioEnabled = 0;
  gPrimaryFrameBuffer = reinterpret_cast<UBYTE*>(raw_fb);
  CSystem* lynx = new CSystem(rom.data(), static_cast<ULONG>(rom.size()),
                              MIKIE_PIXEL_FORMAT_16BPP_565, HANDY_AUDIO_SAMPLE_FREQ);
  if (!lynx || lynx->mFileType == HANDY_FILETYPE_ILLEGAL) {
    gPrimaryFrameBuffer = nullptr;
    delete lynx;
    free(raw_fb);
    printf("[HOST_CORE] ATARI_LYNX STATUS=FAIL (init)\n");
    return 1;
  }
  lynx->mMikie->SetRotation(MIKIE_NO_ROTATE);

  for (int f = 0; f < 50; ++f) {
    lynx->SetButtonData(0);
    lynx->UpdateFrame(true);
  }

  const UBYTE* ram = lynx->GetRamPointer();
  uint8_t marker = ram ? ram[0x80] : 0U;
  int nz = 0;
  for (int i = 0; i < HANDY_SCREEN_WIDTH * HANDY_SCREEN_HEIGHT; ++i) {
    if (raw_fb[i] != 0U) ++nz;
  }

  gPrimaryFrameBuffer = nullptr;
  delete lynx;
  free(raw_fb);

  if (marker != 0x5CU || nz < 100) {
    printf("[HOST_CORE] ATARI_LYNX STATUS=FAIL ram80=0x%02X non_zero=%d\n", marker, nz);
    return 1;
  }
  printf("[HOST_CORE] ATARI_LYNX STATUS=PASS ram80=0x%02X non_zero=%d\n", marker, nz);
  return 0;
}

extern "C" int test_pico_core_cpp(void) {
  static const char kCart[] =
      "pico-8 cartridge // http://www.pico-8.com\n"
      "version 38\n"
      "__lua__\n"
      "function _init()\n"
      "  poke(0x4300, 0x5A)\n"
      "end\n"
      "function _update()\n"
      "  cnt = (cnt or 0) + 1\n"
      "  poke(0x4301, cnt % 256)\n"
      "end\n"
      "function _draw()\n"
      "  cls(1)\n"
      "  rectfill(8, 8, 119, 119, 7)\n"
      "end\n";

  PicoRam* mem = new PicoRam();
  Host* host = new Host(128, 128);
  Graphics* gfx = new Graphics(get_font_data(), mem);
  Input* inp = new Input(mem);
  Audio* aud = new Audio(mem);
  Vm* vm = new Vm(host, mem, gfx, inp, aud);
  host->oneTimeSetup(aud);

  bool loaded = vm->LoadCart(reinterpret_cast<const uint8_t*>(kCart), strlen(kCart), true);
  if (!loaded) {
    delete vm;
    delete aud;
    delete inp;
    delete gfx;
    delete host;
    delete mem;
    printf("[HOST_CORE] PICO_8 STATUS=FAIL (LoadCart)\n");
    return 1;
  }

  vm->vm_run();
  for (int f = 0; f < 50; ++f) {
    vm->Step();
  }

  uint8_t init_marker = mem->data[0x4300];
  uint8_t step_count = mem->data[0x4301];
  int nz = 0;
  for (size_t i = 0; i < 8192U; ++i) {
    if (mem->data[0x6000U + i] != 0U) ++nz;
  }

  delete vm;
  delete aud;
  delete inp;
  delete gfx;
  delete host;
  delete mem;

  if (init_marker != 0x5AU || step_count == 0U || nz < 1000) {
    printf("[HOST_CORE] PICO_8 STATUS=FAIL init=0x%02X steps=%u fb_nz=%d\n",
           init_marker, step_count, nz);
    return 1;
  }
  printf("[HOST_CORE] PICO_8 STATUS=PASS init=0x%02X steps=%u fb_nz=%d\n",
         init_marker, step_count, nz);
  return 0;
}
