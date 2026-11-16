// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "app_menus.h"

#include <Arduino.h>

#include "../hal/buttons.h"
#include "../hal/profiler.h"
#include "../storage/box_art.h"
#include "../storage/sd_card.h"
#include "../ui/bmo_face.h"
#include "../ui/bmo_quotes.h"
#include "../ui/console_catalog.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include "app_launcher.h"
#include "app_state.h"

namespace {

void handleConsoleNavigation(BmoAppRuntime::RuntimeState& rt, const BmoInput::InputFrame& input, unsigned long nowMs) {
  const bool left = input.triggeredOrRepeat(Buttons::LEFT);
  const bool right = input.triggeredOrRepeat(Buttons::RIGHT);
  const bool up = input.edgePressed(Buttons::UP);
  const bool down = input.edgePressed(Buttons::DOWN);
  const bool a = input.edgePressed(Buttons::A);
  const bool select = input.edgePressed(Buttons::SELECT);
  const bool start = input.edgePressed(Buttons::START);

  if (left || right || up || down || a || select || start) {
    rt.lastInputActivityMs = nowMs;
  }
  if (left) {
    rt.selectedConsoleIndex =
        (rt.selectedConsoleIndex - 1 + ConsoleCatalog::CONSOLE_COUNT) % ConsoleCatalog::CONSOLE_COUNT;
    rt.visibleGamesDirty = true;
    BmoFace::setGaze(-0.70f, 0.0f);
    DisplayEmu::triggerCarouselGlide(false);
  }
  if (right) {
    rt.selectedConsoleIndex = (rt.selectedConsoleIndex + 1) % ConsoleCatalog::CONSOLE_COUNT;
    rt.visibleGamesDirty = true;
    BmoFace::setGaze(0.70f, 0.0f);
    DisplayEmu::triggerCarouselGlide(true);
  }
}

void handleConsoleActions(BmoAppRuntime::RuntimeState& rt, const BmoInput::InputFrame& input, unsigned long nowMs) {
  if (input.edgePressed(Buttons::UP)) {
    rt.currentState = BmoApp::STATE_IDLE_MASCOT;
    BmoFace::setExpression(BmoFace::HAPPY);
    BmoFace::say(BmoQuotes::kQuotes[0], 3500);
    Buttons::suppressMenuInputUntil(static_cast<uint32_t>(nowMs + 180U));
  } else if (input.edgePressed(Buttons::DOWN) || input.edgePressed(Buttons::SELECT)) {
    rt.currentState = BmoApp::STATE_CONSOLE_MUSEUM;
    Buttons::suppressMenuInputUntil(static_cast<uint32_t>(nowMs + 180U));
  } else if (input.edgePressed(Buttons::START)) {
    rt.currentState = BmoApp::STATE_DIAGNOSTICS;
    Buttons::suppressMenuInputUntil(static_cast<uint32_t>(nowMs + 180U));
  } else if (input.edgePressed(Buttons::A)) {
    rt.selectedGameIndex = 0;
    rt.visibleGamesDirty = true;
    BmoAppRuntime::rebuildVisibleGames();
    rt.currentState = BmoApp::STATE_GAME_MENU;
    BmoFace::setExpression(BmoFace::IDLE);
    BmoAppRuntime::refreshSelectedBoxArt();
    Buttons::suppressMenuInputUntil(static_cast<uint32_t>(nowMs + 180U));
  }
}

void jumpAlphabeticalLetter(BmoAppRuntime::RuntimeState& rt, int direction) {
  if (rt.visibleGameCount <= 0) return;
  const char curLetter = BmoAppRuntime::romInitialLetter(BmoAppRuntime::selectedGame());
  int targetIdx = rt.selectedGameIndex;
  for (int step = 1; step < rt.visibleGameCount; ++step) {
    const int offset = direction * step;
    const int testIdx = (rt.selectedGameIndex + offset + rt.visibleGameCount) % rt.visibleGameCount;
    const RomFile* testRom = SDCard::getRomInfo(rt.visibleRomIndexes[testIdx]);
    if (testRom && testRom->filename[0] && BmoAppRuntime::romInitialLetter(testRom) != curLetter) {
      targetIdx = testIdx;
      break;
    }
  }
  rt.selectedGameIndex = targetIdx;
  const RomFile* newRom = BmoAppRuntime::selectedGame();
  if (newRom && newRom->filename[0]) {
    DisplayEmu::showAlphaBadge(BmoAppRuntime::romInitialLetter(newRom), 800);
  }
}

void handleGameMenuToggles(BmoAppRuntime::RuntimeState& rt, const BmoInput::InputFrame& input) {
  const bool start = input.edgePressed(Buttons::START);
  const bool select = input.edgePressed(Buttons::SELECT);
  if (start && ConsoleCatalog::CONSOLES[rt.selectedConsoleIndex] != ROM_FAVORITES) {
    rt.topPicksOnly = !rt.topPicksOnly;
    DisplayEmu::setTopPicksFilter(rt.topPicksOnly);
    rt.selectedGameIndex = 0;
    rt.visibleGamesDirty = true;
    BmoAppRuntime::rebuildVisibleGames();
    BmoAppRuntime::refreshSelectedBoxArt();
    DisplayEmu::showToast(rt.topPicksOnly ? "FILTER: TOP PICKS ONLY" : "FILTER: ALL GAMES",
                          rt.topPicksOnly ? Theme::BMO_SCREEN_MINT : Theme::BMO_BODY_TEAL, 1600);
  }
  if (select && rt.visibleGameCount > 0) {
    const int romIdx = rt.visibleRomIndexes[rt.selectedGameIndex];
    SDCard::toggleFavorite(romIdx);
    rt.consoleCountsDirty = true;
    const bool fav = SDCard::isFavorite(romIdx);
    if (fav) {
      BmoFace::triggerWink();
      DisplayEmu::showToast("* ADDED TO FAVORITES", Theme::BMO_DPAD_YELLOW, 1600);
    } else {
      BmoFace::setExpression(BmoFace::IDLE);
      DisplayEmu::showToast("REMOVED FROM FAVORITES", Theme::BMO_BODY_TEAL, 1600);
    }
    if (ConsoleCatalog::CONSOLES[rt.selectedConsoleIndex] == ROM_FAVORITES) {
      rt.visibleGamesDirty = true;
      BmoAppRuntime::rebuildVisibleGames();
      BmoAppRuntime::refreshSelectedBoxArt();
    }
  }
}

void updateGameMenuSelection(BmoAppRuntime::RuntimeState& rt, const BmoInput::InputFrame& input) {
  const int prevGameIndex = rt.selectedGameIndex;
  if (input.triggeredOrRepeat(Buttons::LEFT)) jumpAlphabeticalLetter(rt, -1);
  if (input.triggeredOrRepeat(Buttons::RIGHT)) jumpAlphabeticalLetter(rt, 1);
  if (input.triggeredOrRepeat(Buttons::UP) && rt.visibleGameCount > 0) {
    rt.selectedGameIndex = (rt.selectedGameIndex - 1 + rt.visibleGameCount) % rt.visibleGameCount;
  }
  if (input.triggeredOrRepeat(Buttons::DOWN) && rt.visibleGameCount > 0) {
    rt.selectedGameIndex = (rt.selectedGameIndex + 1) % rt.visibleGameCount;
  }
  if (rt.selectedGameIndex != prevGameIndex) {
    BmoAppRuntime::refreshSelectedBoxArt();
  }
}

void updateIdleMascotGaze(BmoAppRuntime::RuntimeState& rt) {
  float gx = 0.0f;
  float gy = 0.0f;
  if (Buttons::get(Buttons::LEFT).pressed) gx -= 0.85f;
  if (Buttons::get(Buttons::RIGHT).pressed) gx += 0.85f;
  if (Buttons::get(Buttons::UP).pressed) gy += 0.85f;
  if (Buttons::get(Buttons::DOWN).pressed) gy -= 0.85f;
  if (gx == 0.0f && gy == 0.0f) return;

  BmoFace::setGaze(gx, gy);
  rt.lastInputActivityMs = millis();

  static unsigned long lastDirChangeMs = 0;
  static int dirToggleCount = 0;
  static float lastGx = 0.0f;
  if ((gx > 0.0f && lastGx < 0.0f) || (gx < 0.0f && lastGx > 0.0f)) {
    dirToggleCount = (millis() - lastDirChangeMs < 350UL) ? (dirToggleCount + 1) : 0;
    if (dirToggleCount >= 2) {
      BmoFace::petCompanion();
      dirToggleCount = 0;
    }
    lastDirChangeMs = millis();
  }
  lastGx = gx;
}

void updateKonamiSequence(const BmoInput::InputFrame& input) {
  static constexpr int kKonamiSeq[10] = {Buttons::UP,    Buttons::UP,   Buttons::DOWN,  Buttons::DOWN, Buttons::LEFT,
                                         Buttons::RIGHT, Buttons::LEFT, Buttons::RIGHT, Buttons::B,    Buttons::A};
  static int s_konamiStep = 0;
  for (int bi = 0; bi < Buttons::count(); ++bi) {
    if (!input.edgePressed(bi)) continue;
    if (bi == kKonamiSeq[s_konamiStep]) {
      s_konamiStep++;
      if (s_konamiStep == 10) {
        s_konamiStep = 0;
        BmoFace::triggerDance(140.0f);
        BmoFace::triggerWink();
        BmoFace::say("KONAMI CODE! +30 LIVES FOR BMO!", 4500);
        DisplayEmu::showToast("* KONAMI CODE UNLOCKED! *", Theme::BMO_DPAD_YELLOW, 2500);
      }
    } else {
      s_konamiStep = (bi == kKonamiSeq[0]) ? 1 : 0;
    }
    break;
  }
}

void handleIdleMascotInteractions(BmoAppRuntime::RuntimeState& rt, const BmoInput::InputFrame& input) {
  if (input.edgePressed(Buttons::A)) {
    BmoFace::triggerWink();
    BmoFace::say(BmoQuotes::kQuotes[BmoQuotes::kWinkQuoteIndex], 3000);
    rt.lastInputActivityMs = millis();
  } else if (input.edgePressed(Buttons::B)) {
    BmoFace::triggerBlush();
    BmoFace::say(BmoQuotes::kQuotes[BmoQuotes::kBlushQuoteIndex], 3000);
    rt.lastInputActivityMs = millis();
  } else if (input.edgePressed(Buttons::SELECT)) {
    static uint32_t s_quoteIdx = 0;
    BmoFace::setExpression(BmoFace::JOY);
    BmoFace::say(BmoQuotes::kQuotes[s_quoteIdx % BmoQuotes::kIdleQuoteCount], 3800);
    s_quoteIdx++;
    rt.lastInputActivityMs = millis();
  } else if (input.edgePressed(Buttons::UP)) {
    if (BmoFace::isDancing()) {
      BmoFace::stopDance();
      BmoFace::say(BmoQuotes::kQuotes[BmoQuotes::kDanceEndQuoteIndex], 2500);
    } else {
      BmoFace::triggerDance(125.0f);
    }
    rt.lastInputActivityMs = millis();
  }
}

} // namespace

namespace BmoAppMenus {

void handleBootSplash() {
  BmoAppRuntime::RuntimeState& rt = BmoAppRuntime::state();
  const unsigned long splashStart = millis();
  DisplayEmu::initMenuUI();
  Buttons::update(false);
  const auto& input = Buttons::inputFrame();

  if (input.justPressedMask != 0U || (millis() - rt.bootSplashStartMs) > 3500UL) {
    rt.currentState = BmoApp::STATE_CONSOLE_MENU;
    rt.lastInputActivityMs = millis();
    Buttons::suppressMenuInputUntil(static_cast<uint32_t>(millis() + 220U));
    return;
  }

  const bool blinkState = ((millis() / 400UL) % 2UL == 0UL);
  DisplayEmu::drawBootSplash(blinkState);
  BmoAppRuntime::paceMenuFrame(splashStart, 33UL);
}

void handleConsoleMenu() {
  BmoAppRuntime::RuntimeState& rt = BmoAppRuntime::state();
  const unsigned long menuFrameStart = millis();
  DisplayEmu::initMenuUI();
  Buttons::update(true);
  const auto& input = Buttons::inputFrame();
  const unsigned long nowMs = millis();

  handleConsoleNavigation(rt, input, nowMs);
  handleConsoleActions(rt, input, nowMs);

  if (millis() - rt.lastInputActivityMs > BmoAppRuntime::kIdleMascotTimeoutMs) {
    rt.currentState = BmoApp::STATE_IDLE_MASCOT;
    BmoFace::setExpression(BmoFace::SLEEPY);
  }
  if (rt.consoleCountsDirty) {
    for (int i = 0; i < ConsoleCatalog::CONSOLE_COUNT; ++i) {
      rt.cachedConsoleCounts[i] = SDCard::getRomCountForType(ConsoleCatalog::CONSOLES[i]);
    }
    rt.consoleCountsDirty = false;
  }

  BMO_PROFILE_START(ZONE_MENU_RENDER);
  DisplayEmu::drawConsoleSelectMenu(rt.selectedConsoleIndex, rt.cachedConsoleCounts, ConsoleCatalog::CONSOLE_COUNT,
                                    SDCard::isMounted());
  BMO_PROFILE_END(ZONE_MENU_RENDER);
  BmoAppRuntime::paceMenuFrame(menuFrameStart);
}

void handleConsoleMuseum() {
  BmoAppRuntime::RuntimeState& rt = BmoAppRuntime::state();
  const unsigned long museumFrameStart = millis();
  DisplayEmu::initMenuUI();
  Buttons::update(false);
  const auto& input = Buttons::inputFrame();

  if (input.edgePressed(Buttons::B) || input.edgePressed(Buttons::SELECT) || input.edgePressed(Buttons::A)) {
    rt.currentState = BmoApp::STATE_CONSOLE_MENU;
    Buttons::suppressMenuInputUntil(static_cast<uint32_t>(millis() + 180U));
  }

  DisplayEmu::drawConsoleMuseumModal(ConsoleCatalog::CONSOLES[rt.selectedConsoleIndex]);
  BmoAppRuntime::paceMenuFrame(museumFrameStart);
}

void handleGameMenu() {
  BmoAppRuntime::RuntimeState& rt = BmoAppRuntime::state();
  const unsigned long menuFrameStart = millis();
  DisplayEmu::initMenuUI();
  Buttons::update(true);
  const auto& input = Buttons::inputFrame();

  if (rt.visibleGamesDirty) {
    BmoAppRuntime::rebuildVisibleGames();
  }

  handleGameMenuToggles(rt, input);
  updateGameMenuSelection(rt, input);

  if (input.edgePressed(Buttons::B)) {
    BoxArt::unload();
    rt.topPicksOnly = false;
    DisplayEmu::setTopPicksFilter(false);
    rt.currentState = BmoApp::STATE_CONSOLE_MENU;
    BmoFace::setExpression(BmoFace::IDLE);
    Buttons::suppressMenuInputUntil(static_cast<uint32_t>(millis() + 180U));
  } else if (input.edgePressed(Buttons::A) && rt.visibleGameCount > 0) {
    BmoAppLauncher::launchSelectedRom(BmoAppRuntime::selectedGame());
    return;
  }

  BMO_PROFILE_START(ZONE_MENU_RENDER);
  DisplayEmu::drawGameSelectMenu(rt.visibleGames, rt.visibleGameCount, rt.selectedGameIndex,
                                 ConsoleCatalog::CONSOLES[rt.selectedConsoleIndex], SDCard::isMounted());
  BMO_PROFILE_END(ZONE_MENU_RENDER);
  BmoAppRuntime::paceMenuFrame(menuFrameStart);
}

void handleIdleMascot() {
  BmoAppRuntime::RuntimeState& rt = BmoAppRuntime::state();
  const unsigned long idleStart = millis();
  Buttons::update(false);
  const auto& input = Buttons::inputFrame();

  updateIdleMascotGaze(rt);
  updateKonamiSequence(input);
  handleIdleMascotInteractions(rt, input);

  static unsigned long btnHoldStart = 0;
  if (Buttons::get(Buttons::B).pressed) {
    if (btnHoldStart == 0) btnHoldStart = millis();
  } else {
    btnHoldStart = 0;
  }

  if (input.edgePressed(Buttons::START) || (btnHoldStart != 0 && (millis() - btnHoldStart > 600UL))) {
    rt.currentState = BmoApp::STATE_CONSOLE_MENU;
    BmoFace::setExpression(BmoFace::HAPPY);
    BmoFace::clearSpeech();
    rt.lastInputActivityMs = millis();
    Buttons::suppressMenuInputUntil(static_cast<uint32_t>(millis() + 220U));
    btnHoldStart = 0;
  } else {
    const unsigned long idleSec = (millis() - rt.lastInputActivityMs) / 1000UL;
    if (idleSec > 45UL && BmoFace::getExpression() != BmoFace::SLEEPING) {
      BmoFace::setExpression(BmoFace::SLEEPING);
    } else if (idleSec > 20UL && idleSec <= 45UL && BmoFace::getExpression() == BmoFace::IDLE) {
      BmoFace::setExpression(BmoFace::SLEEPY);
    }
    DisplayEmu::drawIdleMascotScreen(idleSec);
  }
  BmoAppRuntime::paceMenuFrame(idleStart);
}

} // namespace BmoAppMenus
