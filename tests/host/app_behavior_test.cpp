#include <Arduino.h>
#include <soc/gpio_reg.h>
#include <stdint.h>

#include "app/app_diagnostics.h"
#include "app/app_launcher.h"
#include "app/app_menus.h"
#include "app/app_state.h"
#include "hal/buttons.h"
#include "hal/pin_map.h"
#include "hal/profiler.h"
#include "storage/box_art.h"
#include "storage/save_manager.h"
#include "storage/sd_card.h"
#include "ui/bmo_face.h"
#include "ui/display_emu.h"
#include "test_check.h"

namespace {
BmoAppRuntime::RuntimeState g_rt;
int g_sdBeginCalls = 0;
int g_sdRebuildCalls = 0;
int g_danceTriggerCount = 0;
float g_danceLastBpm = 0.0f;
int g_toastCount = 0;
uint32_t g_diagSpiClockHz = TFT_SPI_HZ;
uint32_t g_persistedSpiClockHz = 0;
bool g_dancing = false;
BmoFace::BmoExpression g_faceExpr = BmoFace::IDLE;
} // namespace

// Mocks for BmoAppRuntime & external modules linked by app_diagnostics.cpp + app_menus.cpp
namespace BmoAppRuntime {
RuntimeState& state() {
  return g_rt;
}
void syncVisiblePoolCapacity() {}
void persistDisplaySpiClock(uint32_t hz) {
  g_persistedSpiClockHz = hz;
}
void rebuildVisibleGames() {
  g_rt.visibleGamesDirty = false;
}
const RomFile* selectedGame() {
  return nullptr;
}
void refreshSelectedBoxArt() {}
char romInitialLetter(const RomFile*) {
  return 'A';
}
void paceMenuFrame(unsigned long, unsigned long) {}
} // namespace BmoAppRuntime

namespace BmoAppLauncher {
void launchSelectedRom(const RomFile*) {}
} // namespace BmoAppLauncher

bool SDCard::begin() {
  ++g_sdBeginCalls;
  return true;
}
bool SDCard::isMounted() {
  return true;
}
uint32_t SDCard::getClockHz() {
  return 25000000U;
}
void SDCard::rebuildIndex() {
  ++g_sdRebuildCalls;
}
int SDCard::getRomCountForType(RomType) {
  return 5;
}
int SDCard::getSkippedRomCount() {
  return 0;
}
const RomFile* SDCard::getRomInfo(int) {
  return nullptr;
}
void SDCard::toggleFavorite(int) {}
bool SDCard::isFavorite(int) {
  return false;
}

bool SaveManager::begin() {
  return true;
}

void BoxArt::unload() {}

namespace DisplayEmu {
void initMenuUI() {}
void drawBootSplash(bool) {}
void drawConsoleSelectMenu(int, const int*, int, bool) {}
void drawConsoleMuseumModal(RomType) {}
void drawGameSelectMenu(const RomFile* const*, int, int, RomType, bool) {}
void showToast(const char*, uint16_t, unsigned long) {
  ++g_toastCount;
}
void triggerCarouselGlide(bool) {}
void showAlphaBadge(char, unsigned long) {}
void setTopPicksFilter(bool) {}
void drawDiagnosticsDashboard(unsigned long, uint32_t, uint32_t, uint32_t, uint16_t, uint32_t, uint32_t, uint32_t,
                              uint32_t, bool, uint32_t) {}
void drawDiagnosticsProfiler(unsigned long, uint32_t, uint32_t) {}
uint32_t getDisplayStackHighWaterMarkWords() {
  return 512U;
}
void setDiagnosticSpiClock(uint32_t hz) {
  g_diagSpiClockHz = hz;
}
uint32_t getDiagnosticSpiClock() {
  return g_diagSpiClockHz;
}
void drawSpiStressPattern(uint32_t, uint32_t) {}
void drawIdleMascotScreen(unsigned long, const char*) {}
} // namespace DisplayEmu

namespace BmoFace {
void setExpression(BmoExpression expr) {
  g_faceExpr = expr;
}
BmoExpression getExpression() {
  return g_faceExpr;
}
void setGaze(float, float) {}
void triggerWink() {}
void triggerBlush() {}
void say(const char*, unsigned long) {}
void clearSpeech() {}
void triggerDance(float bpm) {
  ++g_danceTriggerCount;
  g_danceLastBpm = bpm;
  g_dancing = true;
}
void stopDance() {
  g_dancing = false;
}
bool isDancing() {
  return g_dancing;
}
void petCompanion() {}
} // namespace BmoFace

namespace {

void setRawGpioButtons(uint16_t activeMask) {
  uint32_t low = 0xFFFFFFFFU;
  uint32_t high = 0xFFFFFFFFU;
  const uint8_t pins[12] = {BTN_UP,    BTN_DOWN,   BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B,
                            BTN_START, BTN_SELECT, BTN_X,    BTN_Y,     BTN_L, BTN_R};
  for (int i = 0; i < 12; ++i) {
    if ((activeMask & (1U << i)) == 0U) continue;
    const uint8_t pin = pins[i];
    if (pin < 32U) {
      low &= ~(1UL << pin);
    } else {
      high &= ~(1UL << (pin - 32U));
    }
  }
  HostRuntimeFake::g_gpioInLow = low;
  HostRuntimeFake::g_gpioInHigh = high;
}

void advanceAndSample(uint16_t mask, uint32_t holdMs = 25U) {
  setRawGpioButtons(mask);
  Buttons::update(false);
  HostRuntimeFake::g_micros += static_cast<uint64_t>(holdMs) * 1000ULL;
}

void driveDiagStep(uint16_t mask) {
  setRawGpioButtons(mask);
  BmoAppDiagnostics::handleDiagnostics();
  HostRuntimeFake::g_micros += 25000ULL;
  setRawGpioButtons(mask);
  BmoAppDiagnostics::handleDiagnostics();
  HostRuntimeFake::g_micros += 5000ULL;
  setRawGpioButtons(0U);
  BmoAppDiagnostics::handleDiagnostics();
  HostRuntimeFake::g_micros += 25000ULL;
  setRawGpioButtons(0U);
  BmoAppDiagnostics::handleDiagnostics();
}

void driveMascotPress(uint16_t mask) {
  setRawGpioButtons(mask);
  BmoAppMenus::handleIdleMascot();
  HostRuntimeFake::g_micros += 25000ULL;
  setRawGpioButtons(mask);
  BmoAppMenus::handleIdleMascot();
}

void driveMascotRelease(uint16_t mask) {
  setRawGpioButtons(mask);
  BmoAppMenus::handleIdleMascot();
  HostRuntimeFake::g_micros += 25000ULL;
  setRawGpioButtons(mask);
  BmoAppMenus::handleIdleMascot();
}

} // namespace

int main() {
  HostRuntimeFake::g_micros = 1000000ULL;
  Buttons::begin();
  BmoProfiler::begin();
  BmoProfiler::reset();

  // 1) Buttons swap-AB and auto-turbo on real GPIO_IN_REG hardware reads
  Buttons::setGameplayMode(true);
  Buttons::setSwapAB(true);
  advanceAndSample(1U << Buttons::A);
  advanceAndSample(0U);
  const uint8_t swapBitCheck = [&]() {
    advanceAndSample(1U << Buttons::A);
    return Buttons::gb_joypad_state;
  }();
  // Active-low gb_joypad_state: A=0x01, B=0x02; pressing A when swapped asserts B (bit 1=0) and leaves A released (bit
  // 0=1)
  BMO_CHECK((swapBitCheck & 0x03U) == 0x01U);
  Buttons::setSwapAB(false);
  advanceAndSample(0U);

  // Turbo with period 2 toggles A bit (0x01) every frame
  Buttons::setTurboEnabled(true, 2);
  advanceAndSample(1U << Buttons::A, 1U);
  const uint8_t t0 = Buttons::gb_joypad_state;
  advanceAndSample(1U << Buttons::A, 1U);
  const uint8_t t1 = Buttons::gb_joypad_state;
  BMO_CHECK(((t0 ^ t1) & 0x01U) != 0U);
  Buttons::setTurboEnabled(false);
  Buttons::setGameplayMode(false);
  advanceAndSample(0U);
  advanceAndSample(0U);

  // 2) Diagnostics SELECT must toggle profiler WITHOUT calling SDCard::begin or rebuildIndex
  g_rt = BmoAppRuntime::RuntimeState{};
  g_rt.currentState = BmoApp::STATE_DIAGNOSTICS;
  g_sdBeginCalls = 0;
  g_sdRebuildCalls = 0;
  driveDiagStep(1U << Buttons::SELECT);
  BMO_CHECK(g_rt.diagShowProfiler);
  BMO_CHECK(g_sdBeginCalls == 0);
  BMO_CHECK(g_sdRebuildCalls == 0);

  driveDiagStep(1U << Buttons::SELECT);
  BMO_CHECK(!g_rt.diagShowProfiler);
  BMO_CHECK(g_sdBeginCalls == 0);
  BMO_CHECK(g_sdRebuildCalls == 0);

  // Diagnostics START explicitly triggers SD rescan without toggling profiler
  driveDiagStep(1U << Buttons::START);
  BMO_CHECK(!g_rt.diagShowProfiler);
  BMO_CHECK(g_sdRebuildCalls == 1);

  // Diagnostics A enables SPI stress pattern & LEFT switches SPI clock to fallback 40 MHz
  driveDiagStep(1U << Buttons::A);
  BMO_CHECK(g_rt.spiStressPattern);
  driveDiagStep(1U << Buttons::LEFT);
  BMO_CHECK(g_diagSpiClockHz == TFT_SPI_FALLBACK_HZ);
  BMO_CHECK(g_persistedSpiClockHz == TFT_SPI_FALLBACK_HZ);

  // 3) Konami sequence handles multi-button edge transitions without breaking early on bit 0
  g_rt.currentState = BmoApp::STATE_IDLE_MASCOT;
  g_rt.lastInputActivityMs = millis();
  g_danceTriggerCount = 0;
  g_toastCount = 0;

  // Step 1: UP
  driveMascotPress(1U << Buttons::UP);
  driveMascotRelease(0U);
  // Step 2+3 in ONE edge frame: UP (bit 0) + DOWN (bit 1) pressed simultaneously!
  driveMascotPress((1U << Buttons::UP) | (1U << Buttons::DOWN));
  driveMascotRelease(0U);
  // Steps 4..10: DOWN, LEFT, RIGHT, LEFT, RIGHT, B, A
  const int rest[7] = {Buttons::DOWN,  Buttons::LEFT, Buttons::RIGHT, Buttons::LEFT,
                       Buttons::RIGHT, Buttons::B,    Buttons::A};
  for (int btn : rest) {
    driveMascotPress(static_cast<uint16_t>(1U << btn));
    driveMascotRelease(0U);
  }
  BMO_CHECK(g_danceTriggerCount >= 1);
  BMO_CHECK(g_danceLastBpm == 140.0f);
  BMO_CHECK(g_toastCount >= 1);

  // 4) Profiler startZone/endZone + getFrameSummary
  BmoProfiler::startZone(ZONE_EMU_FRAME);
  HostRuntimeFake::g_micros += 9500ULL;
  BmoProfiler::endZone(ZONE_EMU_FRAME);
  BmoProfiler::startZone(ZONE_EMU_FRAME);
  HostRuntimeFake::g_micros += 10500ULL;
  BmoProfiler::endZone(ZONE_EMU_FRAME);
  BmoProfilerTypes::FrameSummary profSummary{};
  BMO_CHECK(BmoProfiler::isEnabled());
  BMO_CHECK(BmoProfiler::getFrameSummary(profSummary));
  BMO_CHECK(profSummary.sampleCount == 2U);
  BMO_CHECK(profSummary.avgUs == 10000U);
  return 0;
}
