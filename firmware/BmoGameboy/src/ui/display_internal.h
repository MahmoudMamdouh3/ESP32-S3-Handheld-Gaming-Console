#pragma once

#include "display_emu.h"
#include "console_catalog.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class PSRAMCanvas : public GFXcanvas16 {
public:
  PSRAMCanvas(uint16_t w, uint16_t h) : GFXcanvas16(w, h, false) {
    buffer = (uint16_t*)heap_caps_malloc((size_t)w * (size_t)h * 2U, MALLOC_CAP_SPIRAM);
  }
  ~PSRAMCanvas() {
    if (buffer) {
      heap_caps_free(buffer);
      buffer = nullptr;
    }
  }
};

namespace BmoDisplayInternal {

constexpr uint16_t UI_TEAL = Theme::BMO_BODY_TEAL;
constexpr uint16_t UI_MINT = Theme::BMO_SCREEN_MINT;
constexpr uint16_t UI_DEEP_TEAL = Theme::BMO_DEEP_TEAL;
constexpr uint16_t UI_YELLOW = Theme::BMO_DPAD_YELLOW;
constexpr uint16_t UI_CORAL = Theme::BMO_CORAL_RED;
constexpr uint16_t UI_BLUE = Theme::BMO_BLUE_BUTTON;
constexpr uint16_t UI_WHITE = Theme::BMO_WHITE;
constexpr uint16_t UI_MUTED = Theme::COLOR_TEXT_MUTED;
constexpr uint16_t UI_BLACK = Theme::BMO_DARK_FOREST;

extern Adafruit_ST7789 tft;
extern PSRAMCanvas* menuCanvas;
extern bool topPicksFilter;
extern float carouselOffset;
extern float carouselVel;
extern unsigned long lastCarouselUpdateMs;
extern char alphaBadgeLetter;
extern unsigned long alphaBadgeExpireMs;

void writeMenuCanvas();
int centeredX(const char* str, int displayWidth);
void drawCentered(const char* text, int y, uint16_t color);
void drawFittedCentered(const char* source, int y, int maxWidth, uint16_t color);
void drawFooter(const char* text);
void drawButtonPill(int x, int y, const char* glyph, const char* label, uint16_t pillBg, uint16_t glyphCol,
                    uint16_t labelCol);
bool internalHasActiveToast();
void drawFloatingToastHUD();

} // namespace BmoDisplayInternal
