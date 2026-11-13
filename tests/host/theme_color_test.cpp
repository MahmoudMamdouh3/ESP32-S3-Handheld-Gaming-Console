#include "theme.h"

static_assert(Theme::BMO_SCREEN_MINT == 0xB9E7, "screen mint canvas word");
static_assert(Theme::makePanelColor(206, 245, 228) == 0xE7B9, "screen mint panel word");
static_assert(Theme::BMO_BODY_TEAL == 0xAB9D, "body teal canvas word");
static_assert(Theme::makePanelColor(95, 180, 156) == 0x9DAB, "body teal panel word");
static_assert(Theme::BMO_DEEP_TEAL == 0x4342, "deep teal canvas word");
static_assert(Theme::makePanelColor(26, 75, 66) == 0x4243, "deep teal panel word");
static_assert(Theme::BMO_DARK_FOREST == 0x2121, "dark forest word");
static_assert(Theme::BMO_DPAD_YELLOW == 0x1F37, "d-pad yellow canvas word");
static_assert(Theme::makePanelColor(255, 224, 51) == 0x371F, "d-pad yellow panel word");
static_assert(Theme::BMO_CORAL_RED == 0x9FA4, "coral canvas word");
static_assert(Theme::makePanelColor(255, 145, 165) == 0xA49F, "coral panel word");
static_assert(Theme::BMO_BLUE_BUTTON == 0x2ABD, "blue canvas word");
static_assert(Theme::makePanelColor(82, 166, 186) == 0xBD2A, "blue panel word");
static_assert(Theme::BMO_WHITE == 0xFFFF, "white word");
static_assert(Theme::BMO_MOUTH_GREEN == 0x6955, "green canvas word");
static_assert(Theme::makePanelColor(76, 175, 80) == 0x5569, "green panel word");
static_assert(Theme::BMO_TONGUE_PINK == 0x7EB4, "pink canvas word");
static_assert(Theme::makePanelColor(244, 143, 177) == 0xB47E, "pink panel word");

int main() {
  return 0;
}
