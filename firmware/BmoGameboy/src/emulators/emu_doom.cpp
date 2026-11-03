#pragma GCC optimize ("O3,unroll-loops")
#include "emu_doom.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/spi_arbiter.h"
#include "../core/config.h"

extern "C" {
#include "../vendor/doom/src/doomgeneric.h"
#include "../vendor/doom/src/doomkeys.h"
}

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

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

    void Doom_FreePSRAM(void* ptr) {
        if (ptr) {
            heap_caps_free(ptr);
        }
    }

    void Doom_SpiLock(void) {
        SpiArbiter::lock();
    }

    void Doom_SpiUnlock(void) {
        SpiArbiter::unlock();
    }
}

extern "C" {

// doomgeneric callbacks
void DG_Init() {
  // Nothing to do for init, SPI/Display is already setup
}

void DG_DrawFrame() {
  // DG_ScreenBuffer is an 8-bit palette indexed array of size 320x200.
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

static char s_wadPath[128];
static char* s_argv[4];

bool begin(const char* wadPath) {
  if (!wadPath || wadPath[0] == '\0') return false;

  // Initialize key ring-buffer and per-key edge-detect state.
  memset(s_keyState, 0, sizeof(s_keyState));
  s_keyRingHead = s_keyRingTail = 0;

  // Normalize WAD path for ESP-IDF VFS (Arduino SD mounts FATFS at "/sd").
  if (strncmp(wadPath, "/sd/", 4) == 0) {
    snprintf(s_wadPath, sizeof(s_wadPath), "%s", wadPath);
  } else if (wadPath[0] == '/') {
    snprintf(s_wadPath, sizeof(s_wadPath), "/sd%s", wadPath);
  } else {
    snprintf(s_wadPath, sizeof(s_wadPath), "/sd/%s", wadPath);
  }

  // Validate WAD existence and 12-byte IWAD/PWAD header before invoking doomgeneric_Create
  // so missing or corrupt files return false instead of triggering I_Error -> exit(-1).
  uint8_t hdr[12] = {0};
  size_t bytesRead = 0;
  SpiArbiter::lock();
  FILE* f = fopen(s_wadPath, "rb");
  if (f) {
    bytesRead = fread(hdr, 1, sizeof(hdr), f);
    fclose(f);
  }
  SpiArbiter::unlock();

  if (bytesRead < 12) {
    LOG_ERROR("[DOOM] Failed to open or read WAD header: %s", s_wadPath);
    return false;
  }
  if (memcmp(hdr, "IWAD", 4) != 0 && memcmp(hdr, "PWAD", 4) != 0) {
    LOG_ERROR("[DOOM] Invalid WAD signature in %s", s_wadPath);
    return false;
  }
  int32_t numLumps = 0;
  memcpy(&numLumps, hdr + 4, sizeof(int32_t));
  if (numLumps <= 0) {
    LOG_ERROR("[DOOM] Empty lump table in %s", s_wadPath);
    return false;
  }

  s_argv[0] = (char*)"doom";
  s_argv[1] = (char*)"-iwad";
  s_argv[2] = s_wadPath;
  s_argv[3] = NULL;

  doomgeneric_Create(3, s_argv);
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
  DisplayEmu::waitForDisplay();
  doomgeneric_Destroy();
  memset(s_keyState, 0, sizeof(s_keyState));
  s_keyRingHead = s_keyRingTail = 0;
}

}
