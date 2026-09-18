#pragma GCC optimize ("O3,unroll-loops")
#include "emu_doom.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/config.h"

extern "C" {
#include "../vendor/doom/src/doomgeneric.h"
#include "../vendor/doom/src/doomkeys.h"
}

#include <Arduino.h>
#include <esp_heap_caps.h>

// PERF-H2: Ring-buffer for Doom key events — O(1) dequeue vs O(256) table scan.
// Only 8 physical buttons exist; a 16-slot ring (power-of-2 for fast masking) is plenty.
struct DoomKeyEvent { uint8_t key; uint8_t pressed; };
static DoomKeyEvent s_keyRing[16];
static int          s_keyRingHead = 0;
static int          s_keyRingTail = 0;
// Per-key edge-detect: 0 = idle/released, 1 = pressed/held.
static uint8_t      s_keyState[256];

extern "C" {
    void* Doom_MallocPSRAM(size_t size) {
        void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (!ptr) {
            LOG_ERROR("Failed to allocate %d bytes in PSRAM for DOOM!", (int)size);
        }
        return ptr;
    }
}

extern "C" {

// doomgeneric callbacks
void DG_Init() {
  // Nothing to do for init, SPI/Display is already setup
}

void DG_DrawFrame() {
  // DG_ScreenBuffer is an 8-bit palette indexed array of size 320x200
  // Since we defined CMAP256, we must map it through the palette and draw.
  // We'll write a new method in DisplayEmu to stream an 8-bit paletted buffer.
  DisplayEmu::streamDoomFrame((const uint8_t*)DG_ScreenBuffer);
}

void DG_SleepMs(uint32_t ms) {
  // PERF-M4: Warn if Doom's own pacing fires — it would fight the outer 16 742 µs frame pacer.
  if (ms > 0) {
    LOG_WARN("[DOOM] DG_SleepMs(%u ms) — engine pacing active", (unsigned)ms);
  }
  delay(ms);
}

uint32_t DG_GetTicksMs() {
  return millis();
}

int DG_GetKey(int* pressed, unsigned char* key) {
  // PERF-H2: O(1) ring-buffer dequeue — no 256-entry table scan.
  if (s_keyRingTail == s_keyRingHead) return 0;
  *pressed = s_keyRing[s_keyRingTail].pressed;
  *key     = s_keyRing[s_keyRingTail].key;
  s_keyRingTail = (s_keyRingTail + 1) & 15;
  return 1;
}

void DG_SetWindowTitle(const char * title) {
  // Not used in embedded
}

} // extern "C"

// PERF-H2: Edge-detect + ring enqueue.  Only fires on state transitions (press/release).
static void updateDoomKey(int doomKey, bool pressed) {
  if (pressed) {
    if (s_keyState[doomKey] == 0) {
      s_keyState[doomKey] = 1;
      int next = (s_keyRingHead + 1) & 15;
      if (next != s_keyRingTail) {  // ring not full
        s_keyRing[s_keyRingHead] = { (uint8_t)doomKey, 1 };
        s_keyRingHead = next;
      }
    }
  } else {
    if (s_keyState[doomKey] == 1) {
      s_keyState[doomKey] = 0;
      int next = (s_keyRingHead + 1) & 15;
      if (next != s_keyRingTail) {
        s_keyRing[s_keyRingHead] = { (uint8_t)doomKey, 0 };
        s_keyRingHead = next;
      }
    }
  }
}

namespace DoomEmu {

bool begin(const char* wadPath) {
  // Initialize key ring-buffer and per-key edge-detect state.
  memset(s_keyState, 0, sizeof(s_keyState));
  s_keyRingHead = s_keyRingTail = 0;

  // We must pass the WAD path to doomgeneric using argv
  // e.g. ["doom", "-iwad", "/sd/DOOM1.WAD"]
  static char* argv[] = {
    (char*)"doom",
    (char*)"-iwad",
    (char*)wadPath,
    NULL
  };

  doomgeneric_Create(3, argv);
  return true;
}

void runFrame() {
  // Update button state (handled globally in BmoGameboy.ino loop)

  // Map Gameboy buttons to Doom keys
  // A = Fire (CTRL)
  // B = Use/Open (SPACE)
  // SELECT = Map (TAB)
  // START = Enter (ENTER)
  // DPAD = Arrows
  
  updateDoomKey(KEY_RCTRL, Buttons::get(Buttons::A).pressed);
  updateDoomKey(' ',       Buttons::get(Buttons::B).pressed);
  updateDoomKey(KEY_TAB,   Buttons::get(Buttons::SELECT).pressed);
  updateDoomKey(KEY_ENTER, Buttons::get(Buttons::START).pressed);
  updateDoomKey(KEY_UPARROW, Buttons::get(Buttons::UP).pressed);
  updateDoomKey(KEY_DOWNARROW, Buttons::get(Buttons::DOWN).pressed);
  updateDoomKey(KEY_LEFTARROW, Buttons::get(Buttons::LEFT).pressed);
  updateDoomKey(KEY_RIGHTARROW, Buttons::get(Buttons::RIGHT).pressed);

  // Profile DOOM tick latency (this encompasses all internal SD fread()s).
  // DOOM frames target ~28ms (35Hz). If the tick takes >40ms, SD card reads
  // are likely stalling the SPI bus.
  unsigned long startTick = millis();
  doomgeneric_Tick();
  unsigned long elapsedTick = millis() - startTick;
  
  if (elapsedTick > 40) {
    LOG_WARN("DOOM latency spike detected: %lu ms", elapsedTick);
  }
}

void destroy() {
  // Doom engine doesn't have a clean destroy, we just let it be.
}

}
