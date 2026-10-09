#pragma once
// -----------------------------------------------------------------------
// bmo_face.h  –  Procedural SDF mascot face renderer.
//
// Renders a two-eye / one-mouth cartoon face using 2-D signed distance
// functions evaluated per-pixel, anti-aliased with smoothstep().
// Zero bitmap/sprite assets — every pixel is computed mathematically.
//
// -----------------------------------------------------------------------
// Sizing tunables — adjust these without touching render logic:
//
//   FACE_FB_W / FACE_FB_H  : internal framebuffer dimensions (pixels).
//                            128×128 = 32 KB in DRAM; fits alongside ROM
//                            buffers without touching PSRAM.
//
//   FACE_MENU_X / Y        : top-left corner of the small face in menus.
//   FACE_MENU_SIZE         : pixel width/height of the corner face blit.
//   FACE_LARGE_X / Y       : top-left of the large centered boot/error face.
//   FACE_LARGE_SIZE        : pixel width/height of the large face blit.
// -----------------------------------------------------------------------

#include <stdint.h>

#define FACE_FB_W       320
#define FACE_FB_H       240

// Small corner face used during console/game select menus
#define FACE_MENU_X       4
#define FACE_MENU_Y       4
#define FACE_MENU_SIZE   48

// Large centered face used at boot, error, and shutdown
// Centers a 160×160 blit on the 320×240 display
#define FACE_LARGE_SIZE 160
#define FACE_LARGE_X    ((320 - FACE_LARGE_SIZE) / 2)
#define FACE_LARGE_Y    ((240 - FACE_LARGE_SIZE) / 2)

namespace BmoFace {

  // -----------------------------------------------------------------------
  // BmoExpression — 16-State Emotion Matrix for Living BMO Mascot
  //
  // Transitions between states interpolate smoothly via 2nd-order damped
  // harmonic oscillator springs (squash & stretch, bounce, overshoot)
  // except ERROR, which snaps immediately.
  // -----------------------------------------------------------------------
  enum BmoExpression {
    IDLE = 0,     // neutral, living breathing, subtle saccades, blinks
    SURPRISED,    // wide-open eyes, small round mouth, vertical stretch
    HAPPY,        // squinted curved eyes, large smile with tongue, glowing blush
    SLEEPY,       // half-closed droopy eyes, slow breathing
    LOW_BATTERY,  // tired eyes, slight frown, dim tint
    CHARGING,     // content, calm expression
    ERROR,        // X-eyes + frown; cuts instantly, no interpolation
    SHUTDOWN,     // eyes nearly closed, tiny smile — deep sleep entry
    HIDDEN,       // face not drawn; used during STATE_EMULATOR
    // 2026 Extended Living Mascot Expressions:
    JOY,          // bouncing excited laugh, sparkle eyes
    SLEEPING,     // closed curved eyes, slow deep breathing, floating Z z z
    WINK,         // one eye wink with star glint, playful smile
    BLUSH,        // deep coral blush, eyes looking bashfully away
    CONFUSED,     // one eye raised, crooked mouth
    ANNOYED,      // flat lids, straight mouth
    LOVE,         // beating heart eyes, glowing cheeks
    TICKLED,      // squirming giggling spring bounce
    CELEBRATING   // star bursts, joyful open grin
  };

  // Called once in setup(), after DisplayEmu::begin().
  // Allocates high-definition canvas in Octal PSRAM and initializes physics.
  void begin();

  // Switch to a new target expression with spring-damper dynamics.
  void setExpression(BmoExpression expr);
  BmoExpression getExpression();

  // Non-blocking animation and physics tick — call every loop() iteration.
  void update();

  // Render + blit the face at display position (x, y), drawn directly with
  // subpixel anti-aliasing to 'size' pixels.
  void draw(int x, int y, int size);

  // Zero-argument overload: blits full-screen living BMO face (320x240)
  // directly to display via hardware SPI transaction.
  void draw();

  // Returns true if any animated parameter changed since the last draw().
  bool isDirty();

  // -----------------------------------------------------------------------
  // 2026 Interactive & Living Mascot Extensions
  // -----------------------------------------------------------------------
  void setGaze(float gx, float gy);
  void tickle(float intensity = 1.0f);
  void triggerWink();
  void triggerBlush();
  void triggerJoy();

  // Direct high-performance vector SDF rendering into caller-supplied buffer
  void renderToBuffer(uint16_t* dst, int stride, int x, int y, int w, int h);
  void renderFullScreen(uint16_t* dst, int width = 320, int height = 240);
  uint16_t getScreenBgColor();

} // namespace BmoFace

