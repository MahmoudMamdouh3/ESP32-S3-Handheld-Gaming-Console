#include "display_internal.h"
#include "../hal/spi_arbiter.h"
#include <Fonts/FreeSans9pt7b.h>
#include <cstdio>
#include <cstring>

namespace BmoDisplayInternal {

bool topPicksFilter = false;
float carouselOffset = 0.0f;
float carouselVel = 0.0f;
unsigned long lastCarouselUpdateMs = 0;
char alphaBadgeLetter = '\0';
unsigned long alphaBadgeExpireMs = 0;

} // namespace BmoDisplayInternal

namespace {

using namespace BmoDisplayInternal;

static char s_toastMessage[48] = {0};
static uint16_t s_toastBorderColor = Theme::BMO_DPAD_YELLOW;
static unsigned long s_toastExpireMs = 0;
static float s_toastY = -28.0f;
static float s_toastVelY = 0.0f;
static unsigned long s_lastToastUpdateMs = 0;

static int s_activeDmgPaletteIdx = 1; // Default to authentic BMO Teal (Theme::PALETTE_BMO)
static const uint16_t* const DMG_PALETTES[Theme::PALETTE_COUNT] = {
    DisplayEmu::CLASSIC_PALETTE, Theme::PALETTE_BMO, Theme::PALETTE_POCKET, Theme::PALETTE_LIGHT, Theme::PALETTE_AMBER};

} // namespace

int BmoDisplayInternal::centeredX(const char* str, int displayWidth) {
  int16_t x1, y1;
  uint16_t w, h;
  menuCanvas->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  const int cx = (displayWidth - (int)w) / 2 - x1;
  return (cx < 0) ? 2 : cx;
}

void BmoDisplayInternal::drawCentered(const char* text, int y, uint16_t color) {
  menuCanvas->setCursor(centeredX(text, 320), y);
  menuCanvas->setTextColor(color);
  menuCanvas->print(text);
}

void BmoDisplayInternal::drawFittedCentered(const char* source, int y, int maxWidth, uint16_t color) {
  char fitted[64];
  int len = snprintf(fitted, sizeof(fitted), "%s", source ? source : "Unknown game");
  if (len >= (int)sizeof(fitted)) len = (int)sizeof(fitted) - 1;
  int16_t x1, y1;
  uint16_t width, height;
  menuCanvas->getTextBounds(fitted, 0, 0, &x1, &y1, &width, &height);
  while ((int)width > maxWidth && len > 4) {
    len--;
    fitted[len - 3] = '.';
    fitted[len - 2] = '.';
    fitted[len - 1] = '.';
    fitted[len] = '\0';
    menuCanvas->getTextBounds(fitted, 0, 0, &x1, &y1, &width, &height);
  }
  drawCentered(fitted, y, color);
}

void BmoDisplayInternal::drawFooter(const char* text) {
  menuCanvas->fillRect(0, 212, 320, 28, UI_BLACK);
  menuCanvas->setFont();
  drawFittedCentered(text, 230, 300, UI_MUTED);
}

void BmoDisplayInternal::drawButtonPill(int x, int y, const char* glyph, const char* label, uint16_t pillBg,
                                        uint16_t glyphCol, uint16_t labelCol) {
  const int gLen = (int)strlen(glyph);
  const int pillW = gLen * 6 + 8;
  menuCanvas->fillRoundRect(x, y, pillW, 14, 4, pillBg);
  menuCanvas->setFont();
  menuCanvas->setTextColor(glyphCol);
  menuCanvas->setCursor(x + 4, y + 3);
  menuCanvas->print(glyph);

  menuCanvas->setTextColor(labelCol);
  menuCanvas->setCursor(x + pillW + 4, y + 3);
  menuCanvas->print(label);
}

bool BmoDisplayInternal::internalHasActiveToast() {
  return (s_toastExpireMs > 0 && millis() < s_toastExpireMs) || (s_toastY > -27.0f);
}

void BmoDisplayInternal::drawFloatingToastHUD() {
  if (!menuCanvas || (s_toastExpireMs == 0 && s_toastY <= -27.0f)) return;

  const unsigned long nowMs = millis();
  float dt = (s_lastToastUpdateMs > 0) ? (nowMs - s_lastToastUpdateMs) / 1000.0f : 0.01666f;
  if (dt <= 0.0f || dt > 0.050f) dt = 0.01666f;
  s_lastToastUpdateMs = nowMs;

  const float targetY = (nowMs < s_toastExpireMs) ? 8.0f : -28.0f;
  constexpr float zeta = 0.75f;
  constexpr float omega = 24.0f;
  const float diff = s_toastY - targetY;
  const float acc = -2.0f * zeta * omega * s_toastVelY - (omega * omega) * diff;
  s_toastVelY += acc * dt;
  s_toastY += s_toastVelY * dt;

  if (nowMs >= s_toastExpireMs && s_toastY <= -26.0f) {
    s_toastY = -28.0f;
    s_toastVelY = 0.0f;
    s_toastExpireMs = 0;
    s_toastMessage[0] = '\0';
    return;
  }

  const int curY = (int)s_toastY;
  if (curY + 22 <= 0) return;

  constexpr int toastW = 230;
  constexpr int toastH = 22;
  constexpr int toastX = 45;

  menuCanvas->fillRoundRect(toastX, curY, toastW, toastH, 11, UI_BLACK);
  menuCanvas->drawRoundRect(toastX, curY, toastW, toastH, 11, s_toastBorderColor);
  menuCanvas->fillCircle(toastX + 12, curY + 11, 3, s_toastBorderColor);

  menuCanvas->setFont();
  menuCanvas->setTextColor(UI_WHITE);
  const int textLen = (int)strlen(s_toastMessage);
  const int textW = textLen * 6;
  int textX = toastX + 18 + ((toastW - 24 - textW) / 2);
  if (textX < toastX + 18) textX = toastX + 18;
  menuCanvas->setCursor(textX, curY + 7);
  menuCanvas->print(s_toastMessage);
}

void DisplayEmu::showSDCardWarning() {
  waitForDisplay();
  SpiArbiter::ScopedLock spiLock;
  BmoDisplayInternal::tft.fillRect(40, 80, 240, 80, Theme::PANEL_DEEP_TEAL);
  BmoDisplayInternal::tft.drawRect(42, 82, 236, 76, Theme::PANEL_DPAD_YELLOW);
  BmoDisplayInternal::tft.setFont(&FreeSans9pt7b);
  BmoDisplayInternal::tft.setTextColor(Theme::PANEL_DPAD_YELLOW);
  BmoDisplayInternal::tft.setCursor(55, 115);
  BmoDisplayInternal::tft.print("SD CARD REQUIRED");
  BmoDisplayInternal::tft.setCursor(55, 135);
  BmoDisplayInternal::tft.print("FOR THIS CONSOLE");
}

void DisplayEmu::sanitizeRomTitle(const char* src, char* dst, size_t maxLen) {
  if (!src || !dst || maxLen == 0) return;
  const char* slash = strrchr(src, '/');
  if (!slash) slash = strrchr(src, '\\');
  if (slash) src = slash + 1;
  size_t si = 0, di = 0;
  bool inParen = false, inBracket = false;

  while (src[si] != '\0' && di + 1 < maxLen) {
    char c = src[si++];
    if (c == '(') {
      inParen = true;
      continue;
    }
    if (c == ')') {
      inParen = false;
      continue;
    }
    if (c == '[') {
      inBracket = true;
      continue;
    }
    if (c == ']') {
      inBracket = false;
      continue;
    }
    if (inParen || inBracket) continue;

    if (c == '_') c = ' ';
    if (c == ' ' && di > 0 && dst[di - 1] == ' ') continue;

    dst[di++] = c;
  }
  dst[di] = '\0';

  char* ext = strrchr(dst, '.');
  if (ext) *ext = '\0';

  while (di > 0 && (dst[di - 1] == ' ' || dst[di - 1] == '\t')) {
    dst[--di] = '\0';
  }

  if (di == 0) {
    snprintf(dst, maxLen, "%s", src);
  }
}

void DisplayEmu::setDmgPalette(int paletteIndex) {
  if (paletteIndex >= 0 && paletteIndex < Theme::PALETTE_COUNT) {
    s_activeDmgPaletteIdx = paletteIndex;
  }
}

int DisplayEmu::getDmgPaletteIndex() {
  return s_activeDmgPaletteIdx;
}

void DisplayEmu::cycleDmgPalette() {
  s_activeDmgPaletteIdx = (s_activeDmgPaletteIdx + 1) % Theme::PALETTE_COUNT;
}

const uint16_t* DisplayEmu::getActiveDmgPalette() {
  return DMG_PALETTES[s_activeDmgPaletteIdx];
}

void DisplayEmu::showToast(const char* message, uint16_t borderColor, unsigned long durationMs) {
  if (!message || message[0] == '\0') return;
  strncpy(s_toastMessage, message, sizeof(s_toastMessage) - 1);
  s_toastMessage[sizeof(s_toastMessage) - 1] = '\0';
  s_toastBorderColor = borderColor;
  s_toastExpireMs = millis() + durationMs;
  s_toastY = -28.0f;
  s_toastVelY = 220.0f;
  s_lastToastUpdateMs = millis();
}

bool DisplayEmu::hasActiveToast() {
  return BmoDisplayInternal::internalHasActiveToast();
}

void DisplayEmu::triggerCarouselGlide(bool directionRight) {
  if (directionRight) {
    BmoDisplayInternal::carouselOffset = 36.0f;
    BmoDisplayInternal::carouselVel = -240.0f;
  } else {
    BmoDisplayInternal::carouselOffset = -36.0f;
    BmoDisplayInternal::carouselVel = 240.0f;
  }
  BmoDisplayInternal::lastCarouselUpdateMs = millis();
}

void DisplayEmu::showAlphaBadge(char letter, unsigned long durationMs) {
  if (letter >= 'a' && letter <= 'z') {
    letter = (char)(letter - 'a' + 'A');
  }
  BmoDisplayInternal::alphaBadgeLetter = letter;
  BmoDisplayInternal::alphaBadgeExpireMs = millis() + durationMs;
}

void DisplayEmu::setTopPicksFilter(bool enabled) {
  BmoDisplayInternal::topPicksFilter = enabled;
}

bool DisplayEmu::getTopPicksFilter() {
  return BmoDisplayInternal::topPicksFilter;
}

void DisplayEmu::drawConsoleIcon(RomType type, int x, int y, uint16_t primaryColor) {
  using namespace BmoDisplayInternal;
  if (!menuCanvas) return;
  (void)primaryColor;

  menuCanvas->fillRoundRect(x, y, 58, 74, 8, UI_BLACK);
  menuCanvas->drawRoundRect(x, y, 58, 74, 8, UI_TEAL);

  const int cx = x + 29;
  const int cy = y + 37;

  switch (type) {
  case ROM_FAVORITES:
    menuCanvas->fillTriangle(cx, cy - 22, cx - 18, cy + 18, cx + 18, cy - 6, UI_YELLOW);
    menuCanvas->fillTriangle(cx, cy - 22, cx + 18, cy + 18, cx - 18, cy - 6, UI_YELLOW);
    menuCanvas->fillTriangle(cx - 22, cy - 6, cx + 22, cy - 6, cx, cy + 22, UI_YELLOW);
    menuCanvas->fillCircle(cx - 7, cy - 2, 2, UI_BLACK);
    menuCanvas->fillCircle(cx + 7, cy - 2, 2, UI_BLACK);
    menuCanvas->drawCircle(cx, cy + 2, 4, UI_BLACK);
    menuCanvas->fillRect(cx - 5, cy - 2, 10, 4, UI_YELLOW);
    menuCanvas->fillCircle(cx - 11, cy + 3, 2, UI_CORAL);
    menuCanvas->fillCircle(cx + 11, cy + 3, 2, UI_CORAL);
    break;

  case ROM_GB:
  case ROM_GBC:
    menuCanvas->fillRoundRect(cx - 16, cy - 26, 32, 54, 4, (type == ROM_GB) ? UI_TEAL : UI_BLUE);
    menuCanvas->fillRect(cx - 12, cy - 22, 24, 20, UI_DEEP_TEAL);
    menuCanvas->fillRect(cx - 9, cy - 19, 18, 14, UI_MINT);
    menuCanvas->drawPixel(cx - 4, cy - 14, UI_BLACK);
    menuCanvas->drawPixel(cx + 4, cy - 14, UI_BLACK);
    menuCanvas->drawFastHLine(cx - 2, cy - 10, 4, UI_BLACK);
    menuCanvas->fillRect(cx - 12, cy + 6, 8, 3, UI_YELLOW);
    menuCanvas->fillRect(cx - 10, cy + 4, 4, 7, UI_YELLOW);
    menuCanvas->fillCircle(cx + 8, cy + 7, 2, UI_CORAL);
    menuCanvas->fillCircle(cx + 3, cy + 11, 2, UI_CORAL);
    break;

  case ROM_NES:
    menuCanvas->fillRoundRect(cx - 24, cy - 14, 48, 28, 3, UI_WHITE);
    menuCanvas->fillRect(cx - 21, cy - 11, 42, 22, UI_BLACK);
    menuCanvas->fillRect(cx - 18, cy - 2, 10, 4, UI_WHITE);
    menuCanvas->fillRect(cx - 15, cy - 5, 4, 10, UI_WHITE);
    menuCanvas->fillCircle(cx + 10, cy + 1, 3, UI_CORAL);
    menuCanvas->fillCircle(cx + 17, cy + 1, 3, UI_CORAL);
    break;

  case ROM_SNES:
    menuCanvas->fillRoundRect(cx - 26, cy - 14, 52, 28, 12, UI_WHITE);
    menuCanvas->fillRoundRect(cx - 22, cy - 11, 44, 22, 9, UI_MUTED);
    menuCanvas->fillRect(cx - 18, cy - 2, 8, 3, UI_BLACK);
    menuCanvas->fillRect(cx - 16, cy - 4, 3, 7, UI_BLACK);
    menuCanvas->fillCircle(cx + 14, cy - 5, 2, UI_YELLOW);
    menuCanvas->fillCircle(cx + 9, cy, 2, Theme::COLOR_SUCCESS);
    menuCanvas->fillCircle(cx + 19, cy, 2, UI_BLUE);
    menuCanvas->fillCircle(cx + 14, cy + 5, 2, UI_CORAL);
    break;

  case ROM_GENESIS:
    menuCanvas->fillRoundRect(cx - 24, cy - 14, 48, 28, 10, UI_BLACK);
    menuCanvas->drawRoundRect(cx - 24, cy - 14, 48, 28, 10, UI_MUTED);
    menuCanvas->fillRect(cx - 3, cy - 8, 6, 2, UI_CORAL);
    menuCanvas->fillCircle(cx - 13, cy + 2, 6, UI_MUTED);
    menuCanvas->fillCircle(cx + 6, cy + 5, 2, UI_WHITE);
    menuCanvas->fillCircle(cx + 12, cy + 2, 2, UI_WHITE);
    menuCanvas->fillCircle(cx + 18, cy - 1, 2, UI_WHITE);
    break;

  case ROM_SMS:
  case ROM_GG:
    menuCanvas->fillRoundRect(cx - 25, cy - 16, 50, 32, 6, UI_BLACK);
    menuCanvas->drawRoundRect(cx - 25, cy - 16, 50, 32, 6, UI_TEAL);
    menuCanvas->fillRect(cx - 14, cy - 10, 28, 20, UI_BLUE);
    menuCanvas->fillCircle(cx + 18, cy - 2, 2, UI_CORAL);
    menuCanvas->fillCircle(cx + 18, cy + 5, 2, UI_YELLOW);
    break;

  case ROM_ATARI:
    menuCanvas->fillRoundRect(cx - 16, cy - 4, 32, 26, 4, UI_BLACK);
    menuCanvas->drawRoundRect(cx - 16, cy - 4, 32, 26, 4, UI_YELLOW);
    menuCanvas->drawCircle(cx, cy + 9, 7, UI_CORAL);
    menuCanvas->fillRect(cx - 3, cy - 22, 6, 20, UI_BLACK);
    menuCanvas->fillCircle(cx, cy - 22, 5, UI_CORAL);
    menuCanvas->fillCircle(cx - 10, cy + 2, 3, UI_CORAL);
    break;

  case ROM_PICO8:
    menuCanvas->fillRoundRect(cx - 16, cy - 24, 32, 48, 4, UI_DEEP_TEAL);
    menuCanvas->fillRect(cx - 12, cy - 18, 24, 26, UI_YELLOW);
    menuCanvas->fillRect(cx - 12, cy + 10, 5, 4, UI_CORAL);
    menuCanvas->fillRect(cx - 7, cy + 10, 5, 4, UI_YELLOW);
    menuCanvas->fillRect(cx - 2, cy + 10, 5, 4, Theme::COLOR_SUCCESS);
    menuCanvas->fillRect(cx + 3, cy + 10, 5, 4, UI_BLUE);
    menuCanvas->fillRect(cx + 8, cy + 10, 4, 4, UI_WHITE);
    break;

  case ROM_WAD:
    menuCanvas->fillRoundRect(cx - 16, cy - 18, 32, 36, 6, UI_BLACK);
    menuCanvas->drawRoundRect(cx - 16, cy - 18, 32, 36, 6, UI_CORAL);
    menuCanvas->fillRect(cx - 11, cy - 8, 22, 8, Theme::COLOR_SUCCESS);
    break;

  default:
    menuCanvas->fillRoundRect(cx - 16, cy - 22, 32, 44, 4, UI_BLACK);
    menuCanvas->drawRoundRect(cx - 16, cy - 22, 32, 44, 4, UI_YELLOW);
    menuCanvas->fillRect(cx - 11, cy - 15, 22, 18, UI_TEAL);
    menuCanvas->setFont();
    menuCanvas->setCursor(cx - 8, cy - 4);
    menuCanvas->setTextColor(UI_BLACK);
    menuCanvas->print(ConsoleCatalog::consoleBadge(type));
    break;
  }
}
