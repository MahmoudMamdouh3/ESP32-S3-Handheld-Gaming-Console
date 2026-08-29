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

#define FACE_FB_W       128
#define FACE_FB_H       128

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
  // BmoExpression — the set of named emotional states the face can show.
  //
  // Transitions between states interpolate smoothly via exponential decay
  // except ERROR, which cuts immediately.
  // -----------------------------------------------------------------------
  enum BmoExpression {
    IDLE,         // neutral, eyes open, mild smile — default menu state
    SURPRISED,    // wide-open eyes, small round mouth
    HAPPY,        // squinted eyes, large smile — shown on game launch
    SLEEPY,       // half-closed eyes, subtle smile
    LOW_BATTERY,  // tired eyes, slight frown
    CHARGING,     // content, calm expression
    ERROR,        // X-eyes + frown; cuts instantly, no interpolation
    SHUTDOWN,     // eyes nearly closed, tiny smile — deep sleep entry
    HIDDEN        // face not drawn; used during STATE_EMULATOR
  };

  // Called once in setup(), after DisplayEmu::begin().
  // Seeds the RNG for blink timing and clears the dirty flag.
  void begin();

  // Switch to a new target expression.
  // Parameters ease toward the new target on every update() call.
  // ERROR bypasses easing and snaps immediately.
  void setExpression(BmoExpression expr);

  // Non-blocking animation tick — call every loop() iteration.
  // Internally rate-limited to ~30 fps; extremely cheap when nothing changed.
  void update();

  // Render + blit the face at display position (x, y), drawn into a
  // FACE_FB_W × FACE_FB_H intermediate buffer then scaled to 'size' pixels.
  // Marks the face clean after a successful blit.
  void draw(int x, int y, int size);

  // Zero-argument overload for callers that don't specify position
  // (battery safeShutdown, emulator error handlers).
  // Blits a large centered face at FACE_LARGE_X/Y, FACE_LARGE_SIZE.
  void draw();

  // Returns true if any animated parameter changed since the last draw().
  // Callers should skip draw() when false to avoid wasting an SPI transaction.
  bool isDirty();

} // namespace BmoFace
