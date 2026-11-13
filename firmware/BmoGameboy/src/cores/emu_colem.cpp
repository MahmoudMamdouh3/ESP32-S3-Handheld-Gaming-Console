#pragma GCC optimize("O3,unroll-loops")
#include "emu_colem.h"
#include "../hal/config.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include <Arduino.h>

#if BMO_EXPERIMENTAL_CORES
#include "../hal/buttons.h"
#include "../hal/psram_alloc.h"
#include "../hal/spi_arbiter.h"
#include "../storage/save_manager.h"
#include "../storage/sd_card.h"
#include "../third_party/smsplus/smsplus_bmo.h"
#if FEATURE_SD_CARD
#include <SD.h>
#endif

namespace {
uint16_t* s_panelFb = nullptr; // 256 x 192 panel BGR565 in PSRAM
bool s_isSg1000 = false;
bool s_running = false;

bool tryLoadColecoBios(uint8_t* dst8k) {
#if FEATURE_SD_CARD
  if (!dst8k || !SDCard::isMounted()) return false;
  static const char* const kBiosPaths[] = {"/bios/coleco.rom", "/coleco.rom"};
  DisplayEmu::waitForDisplay();
  SpiArbiter::ScopedLock spiLock;
  bool loaded = false;
  for (const char* path : kBiosPaths) {
    if (!SD.exists(path)) continue;
    File f = SD.open(path, FILE_READ);
    if (f) {
      if (f.size() == 0x2000U && f.read(dst8k, 0x2000U) == 0x2000U) {
        loaded = true;
      }
      f.close();
    }
    if (loaded) break;
  }
  return loaded;
#else
  (void)dst8k;
  return false;
#endif
}
} // namespace

bool ColemEmu::init(uint8_t* romData, size_t romSize, bool isSg1000) {
  destroy();
  if (!romData || romSize == 0) return false;

  s_panelFb = static_cast<uint16_t*>(bmo_psram_calloc(256U * 192U, sizeof(uint16_t)));
  if (!s_panelFb) {
    destroy();
    return false;
  }

  uint8_t* biosTmp = nullptr;
  const uint8_t* biosPtr = nullptr;
  if (!isSg1000) {
    biosTmp = static_cast<uint8_t*>(bmo_psram_malloc(0x2000U));
    if (biosTmp && tryLoadColecoBios(biosTmp)) {
      biosPtr = biosTmp;
    }
  }

  const BmoSmsConsoleMode mode = isSg1000 ? BMO_SMS_CONSOLE_SG1000 : BMO_SMS_CONSOLE_COLECO;
  const bool ok = bmo_sms_init(romData, romSize, mode, biosPtr);
  if (biosTmp) {
    bmo_psram_free(biosTmp);
  }
  if (!ok) {
    destroy();
    return false;
  }

  s_isSg1000 = isSg1000;
  s_running = true;
  return true;
}

void ColemEmu::update() {
  if (!s_running || !s_panelFb || !bmo_sms_is_running()) return;

  int pad = 0;
  if (Buttons::get(Buttons::UP).pressed) pad |= BMO_SMS_INPUT_UP;
  if (Buttons::get(Buttons::DOWN).pressed) pad |= BMO_SMS_INPUT_DOWN;
  if (Buttons::get(Buttons::LEFT).pressed) pad |= BMO_SMS_INPUT_LEFT;
  if (Buttons::get(Buttons::RIGHT).pressed) pad |= BMO_SMS_INPUT_RIGHT;
  if (Buttons::get(Buttons::A).pressed) pad |= BMO_SMS_INPUT_BUTTON1;
  if (Buttons::get(Buttons::B).pressed) pad |= BMO_SMS_INPUT_BUTTON2;

  int sys = 0;
  if (s_isSg1000) {
    if (Buttons::get(Buttons::START).pressed) sys |= BMO_SMS_SYS_PAUSE;
  } else {
    int keypad = 0x0F;
    if (Buttons::get(Buttons::START).pressed)
      keypad = 0x01; // Keypad 1
    else if (Buttons::get(Buttons::X).pressed)
      keypad = 0x02; // Keypad 2
    else if (Buttons::get(Buttons::Y).pressed)
      keypad = 0x03; // Keypad 3
    else if (Buttons::get(Buttons::SELECT).pressed)
      keypad = 0x0A; // Keypad *
    else if (Buttons::get(Buttons::L).pressed)
      keypad = 0x0B; // Keypad #
    sys = (keypad << BMO_SMS_COLECO_KEYPAD_SHIFT);
  }

  bmo_sms_set_input(pad, sys);
  bmo_sms_run_frame(0);
  bmo_sms_render_panel565(s_panelFb);
  DisplayEmu::streamColemFrame(s_panelFb, 256, 192);
}

void ColemEmu::destroy() {
  s_running = false;
  s_isSg1000 = false;
  bmo_sms_destroy();
  if (s_panelFb) {
    bmo_psram_free(s_panelFb);
    s_panelFb = nullptr;
  }
}

bool ColemEmu::isRunning() {
  return s_running && bmo_sms_is_running();
}

bool ColemEmu::saveBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool ColemEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool ColemEmu::saveState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = bmo_sms_state_size();
  if (cap == 0) return false;
  uint8_t* blob = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!blob) return false;

  size_t actualSize = 0;
  bool ok = bmo_sms_save_state(blob, cap, &actualSize) &&
            SaveManager::saveState(romFilename, slot, SaveManager::CORE_COLEM, blob, actualSize);
  bmo_psram_free(blob);
  return ok;
}

bool ColemEmu::loadState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = bmo_sms_state_size();
  if (cap == 0) return false;
  uint8_t* blob = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!blob) return false;

  size_t loadedSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_COLEM, blob, cap, &loadedSize) &&
            bmo_sms_load_state(blob, loadedSize);
  bmo_psram_free(blob);
  return ok;
}

#else // BMO_EXPERIMENTAL_CORES == 0
bool ColemEmu::init(uint8_t* romData, size_t romSize, bool isSg1000) {
  (void)romData;
  (void)romSize;
  (void)isSg1000;
  LOG_WARN_STR("[colem] Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
  DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2200);
  return false;
}

void ColemEmu::update() {}
void ColemEmu::destroy() {}
bool ColemEmu::isRunning() {
  return false;
}
bool ColemEmu::saveBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}
bool ColemEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}
bool ColemEmu::saveState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}
bool ColemEmu::loadState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}

#endif // BMO_EXPERIMENTAL_CORES
