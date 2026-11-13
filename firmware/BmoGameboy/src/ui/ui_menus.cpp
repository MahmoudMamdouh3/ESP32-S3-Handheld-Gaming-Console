#include "display_internal.h"
#include "../hal/battery.h"
#include "../storage/box_art.h"
#include "bmo_face.h"
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <cmath>
#include <cstdio>
#include <cstring>

void DisplayEmu::drawBootSplash(bool pressAnyButtonBlink) {
  using namespace BmoDisplayInternal;
  if (!menuCanvas) return;
  menuCanvas->fillScreen(UI_BLACK);

  constexpr int cx = 160;
  constexpr int cy = 76;

  BmoFace::renderToBuffer(menuCanvas->getBuffer(), 320, cx - 60, cy - 40, 120, 80);
  menuCanvas->drawRoundRect(cx - 61, cy - 41, 122, 82, 10, UI_DEEP_TEAL);

  menuCanvas->setFont(&FreeSans12pt7b);
  drawCentered("BMO HANDHELD", 152, UI_WHITE);

  menuCanvas->setFont();
  menuCanvas->setCursor(34, 178);
  menuCanvas->setTextColor(UI_MINT);
  menuCanvas->print("15 Retro Consoles - Thousands of Adventures");

  if (pressAnyButtonBlink) {
    menuCanvas->setFont(&FreeSans9pt7b);
    drawCentered("PRESS ANY BUTTON", 216, UI_YELLOW);
  }

  writeMenuCanvas();
}

void DisplayEmu::drawConsoleSelectMenu(int selectedIndex, const int* gameCounts, int consoleCount, bool sdMounted) {
  using namespace BmoDisplayInternal;
  if (!menuCanvas || consoleCount <= 0) return;
  if (selectedIndex < 0) selectedIndex = 0;
  if (selectedIndex >= consoleCount) selectedIndex = consoleCount - 1;

  const RomType currentConsole = ConsoleCatalog::CONSOLES[selectedIndex];
  const bool isFav = (currentConsole == ROM_FAVORITES);

  menuCanvas->fillScreen(UI_BLACK);
  menuCanvas->fillRect(0, 0, 320, 42, UI_DEEP_TEAL);

  BmoFace::renderToBuffer(menuCanvas->getBuffer(), 320, 8, 5, 48, 30);
  menuCanvas->drawRoundRect(8, 5, 48, 30, 4, UI_DEEP_TEAL);

  menuCanvas->setFont();
  char counterStr[32];
  snprintf(counterStr, sizeof(counterStr), "SYSTEM %d/%d", selectedIndex + 1, consoleCount);
  menuCanvas->setCursor(220, 20);
  menuCanvas->setTextColor(UI_MINT);
  menuCanvas->print(counterStr);

  if (fabsf(carouselOffset) > 0.05f || fabsf(carouselVel) > 0.5f) {
    const unsigned long nowMs = millis();
    float dt = (lastCarouselUpdateMs > 0) ? (nowMs - lastCarouselUpdateMs) / 1000.0f : 0.01666f;
    if (dt <= 0.0f || dt > 0.050f) dt = 0.01666f;
    lastCarouselUpdateMs = nowMs;

    constexpr float zeta = 0.72f;
    constexpr float omega = 22.0f;
    const float acc = -2.0f * zeta * omega * carouselVel - (omega * omega) * carouselOffset;
    carouselVel += acc * dt;
    carouselOffset += carouselVel * dt;

    if (fabsf(carouselOffset) < 0.2f && fabsf(carouselVel) < 1.0f) {
      carouselOffset = 0.0f;
      carouselVel = 0.0f;
    }
  } else {
    carouselOffset = 0.0f;
    carouselVel = 0.0f;
    lastCarouselUpdateMs = millis();
  }
  const int cardOffset = (int)carouselOffset;

  menuCanvas->fillRoundRect(-18 + cardOffset, 64, 40, 128, 8, UI_DEEP_TEAL);
  menuCanvas->drawRoundRect(-18 + cardOffset, 64, 40, 128, 8, UI_TEAL);
  menuCanvas->fillRoundRect(298 + cardOffset, 64, 40, 128, 8, UI_DEEP_TEAL);
  menuCanvas->drawRoundRect(298 + cardOffset, 64, 40, 128, 8, UI_TEAL);

  const int cardX = 35 + cardOffset;
  constexpr int cardY = 54;
  constexpr int cardW = 250;
  constexpr int cardH = 148;

  menuCanvas->fillRoundRect(cardX, cardY, cardW, cardH, 12, UI_DEEP_TEAL);
  menuCanvas->drawRoundRect(cardX, cardY, cardW, cardH, 12, UI_YELLOW);
  menuCanvas->drawRoundRect(cardX + 1, cardY + 1, cardW - 2, cardH - 2, 11, UI_YELLOW);

  menuCanvas->fillRoundRect(cardX + 16, cardY + 16, 54, 24, 6, UI_YELLOW);
  menuCanvas->setFont();
  menuCanvas->setCursor(cardX + 24, cardY + 24);
  menuCanvas->setTextColor(UI_BLACK);
  menuCanvas->print(ConsoleCatalog::consoleBadge(currentConsole));

  menuCanvas->setCursor(cardX + cardW - 65, cardY + 24);
  menuCanvas->setTextColor(UI_YELLOW);
  menuCanvas->print(isFav ? "Starred" : ConsoleCatalog::consoleYear(currentConsole));

  menuCanvas->setFont(&FreeSans9pt7b);
  menuCanvas->setCursor(cardX + 16, cardY + 74);
  menuCanvas->setTextColor(UI_WHITE);
  menuCanvas->print(isFav ? "* Favorites" : ConsoleCatalog::consoleName(currentConsole));

  menuCanvas->setFont();
  menuCanvas->setCursor(cardX + 16, cardY + 95);
  menuCanvas->setTextColor(UI_MINT);
  char fmtBuf[32];
  snprintf(fmtBuf, sizeof(fmtBuf), "Format: %s", isFav ? "*" : ConsoleCatalog::consoleExt(currentConsole));
  menuCanvas->print(fmtBuf);

  menuCanvas->setCursor(cardX + 16, cardY + 124);
  const bool isDoomDisabled = (currentConsole == ROM_WAD && !BMO_ENABLE_DOOM);
  const bool isExpDisabled = (currentConsole >= ROM_SMS && !BMO_EXPERIMENTAL_CORES);
  if (isDoomDisabled || isExpDisabled) {
    menuCanvas->setTextColor(UI_CORAL);
    menuCanvas->print(isDoomDisabled ? "* DOOM (OFF)" : "* EXPERIMENTAL (OFF)");
  } else {
    menuCanvas->setTextColor(UI_YELLOW);
    char countBuf[32];
    snprintf(countBuf, sizeof(countBuf), "* %d Games Ready", gameCounts[selectedIndex]);
    menuCanvas->print(countBuf);
  }

  drawConsoleIcon(currentConsole, cardX + 175, cardY + 50, UI_YELLOW);

  menuCanvas->setFont(&FreeSans12pt7b);
  menuCanvas->setCursor(14, 134);
  menuCanvas->setTextColor(UI_TEAL);
  menuCanvas->print("<");
  menuCanvas->setCursor(298, 134);
  menuCanvas->print(">");

  menuCanvas->fillRect(0, 214, 320, 26, UI_DEEP_TEAL);
  menuCanvas->drawFastHLine(0, 213, 320, UI_TEAL);
  if (!sdMounted) {
    menuCanvas->setFont();
    menuCanvas->setCursor(20, 224);
    menuCanvas->setTextColor(UI_YELLOW);
    menuCanvas->print("BUILT-IN GAMES ONLY - SD CARD NOT FOUND");
  } else {
    drawButtonPill(10, 220, "A", "PLAY", UI_CORAL, UI_BLACK, UI_WHITE);
    drawButtonPill(88, 220, "UP", "BMO", UI_MINT, UI_BLACK, UI_WHITE);
    drawButtonPill(160, 220, "SEL", "SPECS", UI_TEAL, UI_BLACK, UI_WHITE);
    drawButtonPill(238, 220, "< >", "SYSTEM", UI_YELLOW, UI_BLACK, UI_WHITE);
  }

  writeMenuCanvas();
}

void DisplayEmu::drawConsoleMuseumModal(RomType console) {
  using namespace BmoDisplayInternal;
  if (!menuCanvas) return;
  menuCanvas->fillScreen(UI_BLACK);
  menuCanvas->drawRoundRect(8, 8, 304, 224, 8, UI_YELLOW);
  menuCanvas->fillRect(0, 0, 320, 36, UI_DEEP_TEAL);

  menuCanvas->setFont(&FreeSans9pt7b);
  char title[48];
  snprintf(title, sizeof(title), "SPECS: %s", ConsoleCatalog::consoleName(console));
  drawCentered(title, 24, UI_YELLOW);

  menuCanvas->setFont();
  menuCanvas->setTextColor(UI_WHITE);

  menuCanvas->setCursor(18, 48);
  menuCanvas->printf("Released: %s  |  Badge: %s", ConsoleCatalog::consoleYear(console),
                     ConsoleCatalog::consoleBadge(console));

  const ConsoleCatalog::ConsoleSpec* spec = ConsoleCatalog::findByType(console);
  if (spec) {
    constexpr int kLineY[5] = {66, 80, 100, 114, 134};
    const char* const lines[5] = {
        spec->museumLine1, spec->museumLine2, spec->museumLine3, spec->museumLine4, spec->museumLine5,
    };
    for (int i = 0; i < 5; ++i) {
      menuCanvas->setCursor(18, kLineY[i]);
      menuCanvas->print(lines[i]);
    }
  } else {
    menuCanvas->setCursor(18, 66);
    menuCanvas->print("Historic gaming hardware architecture.");
  }

  drawFooter("PRESS B OR SELECT TO RETURN");
  writeMenuCanvas();
}

void DisplayEmu::drawGameSelectMenu(const RomFile* const* games, int count, int selectedIndex, RomType console,
                                    bool sdMounted) {
  using namespace BmoDisplayInternal;
  if (!menuCanvas) return;
  if (count < 0) count = 0;
  if (count > 0 && (selectedIndex < 0 || selectedIndex >= count)) selectedIndex = 0;

  menuCanvas->fillScreen(UI_BLACK);
  menuCanvas->fillRect(0, 0, 320, 38, UI_DEEP_TEAL);

  BmoFace::renderToBuffer(menuCanvas->getBuffer(), 320, 8, 4, 48, 30);
  menuCanvas->drawRoundRect(8, 4, 48, 30, 4, UI_DEEP_TEAL);

#if FEATURE_BATTERY_MONITOR
  const int pct = Battery::getPercentage();
  const uint16_t battColor = (pct > 50) ? Theme::COLOR_SUCCESS : ((pct > 20) ? UI_YELLOW : UI_CORAL);
  menuCanvas->drawRoundRect(280, 13, 24, 12, 2, UI_WHITE);
  menuCanvas->fillRect(304, 16, 2, 6, UI_WHITE);
  const int fillW = (int)((pct / 100.0f) * 20);
  if (fillW > 0) {
    menuCanvas->fillRect(282, 15, fillW, 8, battColor);
  }
#else
  menuCanvas->drawRoundRect(280, 13, 30, 12, 2, UI_MINT);
  menuCanvas->fillRect(282, 15, 10, 8, UI_MINT);
  menuCanvas->drawRect(294, 15, 10, 8, UI_WHITE);
  menuCanvas->setFont();
  menuCanvas->setTextColor(UI_WHITE);
  menuCanvas->setCursor(286, 15);
  menuCanvas->print("U");
  menuCanvas->setTextColor(UI_MINT);
  menuCanvas->setCursor(255, 16);
  menuCanvas->print("N/A");
#endif

  menuCanvas->setFont(&FreeSans9pt7b);
  char heading[40];
  if (console == ROM_FAVORITES) {
    snprintf(heading, sizeof(heading), "* FAVORITE GAMES");
  } else if (topPicksFilter) {
    snprintf(heading, sizeof(heading), "%s TOP PICKS", ConsoleCatalog::consoleName(console));
  } else {
    snprintf(heading, sizeof(heading), "%s LIBRARY", ConsoleCatalog::consoleName(console));
  }
  drawCentered(heading, 25, UI_YELLOW);

  if (count == 0) {
    menuCanvas->setFont(&FreeSans12pt7b);
    if (console == ROM_FAVORITES) {
      drawCentered("NO FAVORITES YET!", 100, UI_WHITE);
      menuCanvas->setFont(&FreeSans9pt7b);
      drawCentered("Press SELECT on any game in any", 128, UI_MINT);
      drawCentered("console library to star it *", 148, UI_MINT);
    } else {
      drawCentered("NO GAMES", 100, UI_WHITE);
      menuCanvas->setFont(&FreeSans9pt7b);
      drawCentered(sdMounted ? "Add compatible ROMs to the SD card." : "Insert an SD card to add more games.", 138,
                   UI_MINT);
    }
    if (SDCard::getSkippedRomCount() > 0) {
      char skipped[40];
      snprintf(skipped, sizeof(skipped), "SKIPPED PATHS: %d", SDCard::getSkippedRomCount());
      menuCanvas->setFont();
      drawCentered(skipped, 174, UI_YELLOW);
    }
    drawFooter("B: BACK");
    writeMenuCanvas();
    return;
  }

  const RomFile* game = games[selectedIndex];
  const bool isFav = game ? game->isFavorite : false;
  const bool isPop = game ? game->isPopular : false;
  const RomType actualType = game ? game->type : console;

  char title[64];
  sanitizeRomTitle(game ? game->filename : nullptr, title, sizeof(title));

  menuCanvas->fillRoundRect(81, 46, 158, 114, 12, UI_DEEP_TEAL);
  menuCanvas->drawRoundRect(81, 46, 158, 114, 12, isFav ? UI_YELLOW : UI_TEAL);

  if (BoxArt::hasArt()) {
    BoxArt::draw(menuCanvas, 128, 54, true, isFav ? UI_YELLOW : UI_TEAL);
  } else {
    menuCanvas->fillRect(102, 62, 116, 52, UI_BLACK);
    menuCanvas->setFont(&FreeSans12pt7b);
    drawCentered(ConsoleCatalog::consoleBadge(actualType), 98, UI_YELLOW);
  }

  menuCanvas->drawRoundRect(112, 126, 96, 18, 8, isFav ? UI_YELLOW : (isPop ? UI_MINT : UI_TEAL));

  menuCanvas->setFont();
  if (isFav) {
    menuCanvas->setTextColor(UI_YELLOW);
    menuCanvas->setCursor(130, 132);
    menuCanvas->print("* FAVORITE");
  } else if (isPop) {
    menuCanvas->setTextColor(UI_MINT);
    menuCanvas->setCursor(136, 132);
    menuCanvas->print("TOP PICK");
  } else {
    menuCanvas->setTextColor(UI_WHITE);
    menuCanvas->setCursor(126, 132);
    menuCanvas->print(ConsoleCatalog::consoleBadge(actualType));
  }

  menuCanvas->setFont(&FreeSans9pt7b);
  drawFittedCentered(title, 180, 290, UI_WHITE);

  menuCanvas->setFont();
  char position[36];
  snprintf(position, sizeof(position), "%d / %d  [%s%s]", selectedIndex + 1, count,
           ConsoleCatalog::consoleBadge(actualType), topPicksFilter ? " TOP" : "");
  drawCentered(position, 198, UI_MINT);

  if (SDCard::getSkippedRomCount() > 0) {
    char skipped[40];
    snprintf(skipped, sizeof(skipped), "SKIP %d", SDCard::getSkippedRomCount());
    menuCanvas->setFont();
    menuCanvas->setTextColor(UI_YELLOW);
    menuCanvas->setCursor(6, 209);
    menuCanvas->print(skipped);
  }

  if (count > 1) {
    constexpr int trackX = 60;
    constexpr int trackW = 200;
    constexpr int trackY = 168;
    menuCanvas->fillRect(trackX, trackY, trackW, 2, UI_DEEP_TEAL);
    const int rawThumbW = trackW / count;
    const int thumbW = (rawThumbW < 14) ? 14 : rawThumbW;
    const int thumbX = trackX + (int)(((float)selectedIndex / (float)(count - 1)) * (float)(trackW - thumbW));
    menuCanvas->fillRoundRect(thumbX, trackY - 1, thumbW, 4, 2, isFav ? UI_YELLOW : UI_MINT);
  }

  menuCanvas->fillRect(0, 214, 320, 26, UI_DEEP_TEAL);
  menuCanvas->drawFastHLine(0, 213, 320, UI_TEAL);
  drawButtonPill(6, 220, "A", "PLAY", UI_CORAL, UI_BLACK, UI_WHITE);
  drawButtonPill(56, 220, "B", "BACK", UI_MINT, UI_BLACK, UI_WHITE);
  drawButtonPill(106, 220, "SEL", isFav ? "UNSTAR" : "STAR", UI_YELLOW, UI_BLACK, UI_WHITE);
  drawButtonPill(180, 220, "ST", topPicksFilter ? "ALL" : "TOP", UI_BLUE, UI_WHITE, UI_WHITE);
  drawButtonPill(236, 220, "<>", "A-Z", UI_TEAL, UI_BLACK, UI_WHITE);

  if (alphaBadgeExpireMs > millis() && alphaBadgeLetter != '\0') {
    constexpr int cx = 270;
    constexpr int cy = 115;
    constexpr int r = 18;

    menuCanvas->fillCircle(cx, cy, r + 1, UI_BLACK);
    menuCanvas->fillCircle(cx, cy, r, UI_YELLOW);
    menuCanvas->drawCircle(cx, cy, r, UI_WHITE);

    menuCanvas->setFont(&FreeSans9pt7b);
    menuCanvas->setTextColor(UI_BLACK);
    const char badgeStr[2] = {alphaBadgeLetter, '\0'};
    int16_t bx, by;
    uint16_t bw, bh;
    menuCanvas->getTextBounds(badgeStr, 0, 0, &bx, &by, &bw, &bh);
    menuCanvas->setCursor(cx - ((int)bw / 2) - bx, cy - ((int)bh / 2) - by);
    menuCanvas->print(badgeStr);
  }

  writeMenuCanvas();
}

void DisplayEmu::drawPauseMenu(const char* romTitle, int currentSlot, bool hasSaveState, bool hasBatterySave,
                               bool batterySaveSupported, int selectedOption, const char* statusToast) {
  using namespace BmoDisplayInternal;
  if (!menuCanvas) return;

  menuCanvas->fillScreen(UI_BLACK);

  menuCanvas->fillRoundRect(25, 14, 270, 212, 12, UI_DEEP_TEAL);
  menuCanvas->drawRoundRect(25, 14, 270, 212, 12, UI_TEAL);

  menuCanvas->setFont(&FreeSans9pt7b);
  drawCentered("BMO QUICK SAVE & PAUSE", 36, UI_MINT);

  char cleanTitle[48];
  sanitizeRomTitle(romTitle ? romTitle : "Game", cleanTitle, sizeof(cleanTitle));
  menuCanvas->setFont();
  drawFittedCentered(cleanTitle, 48, 240, UI_YELLOW);

  menuCanvas->fillRoundRect(50, 58, 220, 18, 6, UI_BLACK);
  menuCanvas->drawRoundRect(50, 58, 220, 18, 6, hasSaveState ? UI_MINT : UI_MUTED);

  char slotStatus[48];
  snprintf(slotStatus, sizeof(slotStatus), "ACTIVE SLOT: [ %d / 5 ]  %s", currentSlot,
           hasSaveState ? "[SAVED]" : "[EMPTY]");
  menuCanvas->setTextColor(hasSaveState ? UI_MINT : UI_MUTED);
  menuCanvas->setCursor(centeredX(slotStatus, 320), 63);
  menuCanvas->print(slotStatus);

  menuCanvas->drawFastHLine(35, 82, 250, UI_TEAL);

  static const char* const options[6] = {"RESUME GAME",
                                         "QUICK SAVE TO ACTIVE SLOT",
                                         "QUICK LOAD FROM ACTIVE SLOT",
                                         "CYCLE NEXT SLOT (1 - 5)",
                                         "SAVE BATTERY RAM (.SAV)",
                                         "QUIT TO CONSOLE MENU"};

  constexpr int startY = 88;
  constexpr int rowH = 18;

  for (int i = 0; i < 6; i++) {
    const int optY = startY + i * rowH;
    const bool isSel = (i == selectedOption);

    if (isSel) {
      menuCanvas->fillRoundRect(35, optY - 2, 250, 16, 4, UI_TEAL);
      menuCanvas->setTextColor(UI_BLACK);
    } else {
      menuCanvas->setTextColor(UI_WHITE);
    }

    char optStr[48];
    if (i == 1) {
      snprintf(optStr, sizeof(optStr), "%s SAVE [SLOT %d]", isSel ? ">" : " ", currentSlot);
    } else if (i == 2) {
      snprintf(optStr, sizeof(optStr), "%s LOAD [SLOT %d]%s", isSel ? ">" : " ", currentSlot,
               hasSaveState ? "" : " (N/A)");
    } else if (i == 4) {
      if (batterySaveSupported) {
        snprintf(optStr, sizeof(optStr), "%s SAVE BATTERY RAM (.SAV)%s", isSel ? ">" : " ",
                 hasBatterySave ? " [OK]" : "");
      } else {
        snprintf(optStr, sizeof(optStr), "%s NO BATTERY SAVE", isSel ? ">" : " ");
      }
    } else {
      snprintf(optStr, sizeof(optStr), "%s %s", isSel ? ">" : " ", options[i]);
    }

    menuCanvas->setCursor(42, optY + 2);
    menuCanvas->print(optStr);
  }

  if (statusToast && statusToast[0] != '\0') {
    showToast(statusToast, UI_YELLOW, 1800);
  }

  menuCanvas->setTextColor(UI_MUTED);
  drawFittedCentered("L:A/B  R:BEZEL  SEL:SHOT  A:OK  B:PLAY", 206, 260, UI_MUTED);

  writeMenuCanvas();
}

void DisplayEmu::drawIdleMascotScreen(unsigned long idleSeconds, const char* stateMessage) {
  using namespace BmoDisplayInternal;
  if (!menuCanvas) return;

  BmoFace::renderFullScreen(menuCanvas->getBuffer(), 320, 240);

  if (BmoFace::hasSpeech()) {
    const char* quote = BmoFace::getCurrentQuote();
    int visibleCount = BmoFace::getVisibleChars();
    char displayBuf[64];
    if (visibleCount > (int)sizeof(displayBuf) - 1) {
      visibleCount = (int)sizeof(displayBuf) - 1;
    }
    strncpy(displayBuf, quote, (size_t)visibleCount);
    displayBuf[visibleCount] = '\0';

    menuCanvas->setFont();
    menuCanvas->setTextColor(UI_BLACK);
    const int textW = visibleCount * 6;
    const int rawX = 160 - textW / 2;
    const int textX = (rawX < 36) ? 36 : rawX;
    menuCanvas->setCursor(textX, 31);
    menuCanvas->print(displayBuf);
  } else {
    menuCanvas->fillRect(40, 8, 240, 22, UI_DEEP_TEAL);
    menuCanvas->drawRect(40, 8, 240, 22, UI_MINT);
    menuCanvas->setFont(&FreeSans9pt7b);
    const char* title = BmoFace::isDancing() ? "* BMO DANCE PARTY! *"
                        : (idleSeconds > 40) ? "BMO IS SLEEPING..."
                        : (idleSeconds > 20) ? "BMO IS SLEEPY..."
                                             : "LIVING BMO COMPANION";
    drawCentered(title, 24, UI_YELLOW);
  }

  menuCanvas->fillRect(6, 214, 308, 22, UI_BLACK);
  menuCanvas->drawRect(6, 214, 308, 22, UI_TEAL);
  menuCanvas->setFont();
  menuCanvas->setTextColor(UI_MINT);
  const char* prompt = stateMessage ? stateMessage : "D-PAD:LOOK/PET  A:WINK  B:BLUSH  SEL:TALK  UP:DANCE  START:BACK";
  menuCanvas->setCursor(centeredX(prompt, 320), 221);
  menuCanvas->print(prompt);

  writeMenuCanvas();
}
