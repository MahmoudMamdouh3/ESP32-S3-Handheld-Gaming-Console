#pragma GCC optimize("O3,unroll-loops")
#include "emu_sms.h"
#include "../hal/config.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include <Arduino.h>

#if BMO_EXPERIMENTAL_CORES
#include "../hal/buttons.h"
#include "../hal/psram_alloc.h"
#include "../storage/save_manager.h"
#include "../third_party/smsplus/smsplus_bmo.h"

namespace {
uint16_t* s_panelFb = nullptr; // 256 x 192 panel BGR565 in PSRAM
bool s_isGameGear = false;
bool s_running = false;
} // namespace

bool SmsEmu::init(const uint8_t* romData, size_t romSize, bool isGameGear) {
  destroy();
  if (!romData || romSize == 0) return false;

  s_panelFb = static_cast<uint16_t*>(bmo_psram_calloc(256U * 192U, sizeof(uint16_t)));
  if (!s_panelFb) {
    destroy();
    return false;
  }

  const BmoSmsConsoleMode mode = isGameGear ? BMO_SMS_CONSOLE_GG : BMO_SMS_CONSOLE_SMS;
  if (!bmo_sms_init(romData, romSize, mode, nullptr)) {
    destroy();
    return false;
  }

  s_isGameGear = isGameGear;
  s_running = true;
  return true;
}

void SmsEmu::update() {
  if (!s_running || !s_panelFb || !bmo_sms_is_running()) return;

  int pad = 0;
  if (Buttons::get(Buttons::UP).pressed) pad |= BMO_SMS_INPUT_UP;
  if (Buttons::get(Buttons::DOWN).pressed) pad |= BMO_SMS_INPUT_DOWN;
  if (Buttons::get(Buttons::LEFT).pressed) pad |= BMO_SMS_INPUT_LEFT;
  if (Buttons::get(Buttons::RIGHT).pressed) pad |= BMO_SMS_INPUT_RIGHT;
  if (Buttons::get(Buttons::A).pressed) pad |= BMO_SMS_INPUT_BUTTON1;
  if (Buttons::get(Buttons::B).pressed) pad |= BMO_SMS_INPUT_BUTTON2;

  int sys = 0;
  if (Buttons::get(Buttons::START).pressed) {
    sys |= s_isGameGear ? BMO_SMS_SYS_START : BMO_SMS_SYS_PAUSE;
  }

  bmo_sms_set_input(pad, sys);
  bmo_sms_run_frame(0);
  bmo_sms_render_panel565(s_panelFb);
  DisplayEmu::streamSMSFrame(s_panelFb, s_isGameGear);
}

void SmsEmu::destroy() {
  s_running = false;
  s_isGameGear = false;
  bmo_sms_destroy();
  if (s_panelFb) {
    bmo_psram_free(s_panelFb);
    s_panelFb = nullptr;
  }
}

bool SmsEmu::hasActiveBattery() {
  if (!s_running) return false;
  size_t sramSize = 0;
  uint8_t* sram = bmo_sms_sram_data(&sramSize);
  return sram && sramSize > 0 && bmo_sms_sram_used();
}

bool SmsEmu::saveBatteryRam(const char* romFilename) {
  if (!s_running || !romFilename) return false;
  size_t sramSize = 0;
  uint8_t* sram = bmo_sms_sram_data(&sramSize);
  if (!sram || sramSize == 0 || !bmo_sms_sram_used()) return false;
  return SaveManager::saveBatteryRam(romFilename, sram, sramSize);
}

bool SmsEmu::loadBatteryRam(const char* romFilename) {
  if (!s_running || !romFilename) return false;
  size_t sramSize = 0;
  uint8_t* sram = bmo_sms_sram_data(&sramSize);
  if (!sram || sramSize == 0) return false;
  return SaveManager::loadBatteryRam(romFilename, sram, sramSize);
}

bool SmsEmu::saveState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = bmo_sms_state_size();
  if (cap == 0) return false;
  uint8_t* blob = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!blob) return false;

  size_t actualSize = 0;
  bool ok = bmo_sms_save_state(blob, cap, &actualSize) &&
            SaveManager::saveState(romFilename, slot, SaveManager::CORE_SMS, blob, actualSize);
  bmo_psram_free(blob);
  return ok;
}

bool SmsEmu::loadState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = bmo_sms_state_size();
  if (cap == 0) return false;
  uint8_t* blob = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!blob) return false;

  size_t loadedSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_SMS, blob, cap, &loadedSize) &&
            bmo_sms_load_state(blob, loadedSize);
  bmo_psram_free(blob);
  return ok;
}

#else // BMO_EXPERIMENTAL_CORES == 0
bool SmsEmu::init(const uint8_t* romData, size_t romSize, bool isGameGear) {
  (void)isGameGear;
  (void)romData;
  (void)romSize;
  LOG_WARN_STR("[sms] Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
  DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2200);
  return false;
}

void SmsEmu::update() {}
void SmsEmu::destroy() {}
bool SmsEmu::hasActiveBattery() {
  return false;
}

bool SmsEmu::saveBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}
bool SmsEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}
bool SmsEmu::saveState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}
bool SmsEmu::loadState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}

#endif // BMO_EXPERIMENTAL_CORES
