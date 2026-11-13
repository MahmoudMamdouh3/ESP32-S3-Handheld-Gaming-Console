#pragma GCC optimize ("O3,unroll-loops")
#include "emu_doom.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/spi_arbiter.h"
#include "../core/config.h"
#include "../core/theme.h"

#include <Arduino.h>
#include <string.h>

#if BMO_ENABLE_DOOM

extern "C" {
#include "../vendor/doom/src/doomgeneric.h"
#include "../vendor/doom/src/doomkeys.h"
#include "../vendor/doom/src/m_controls.h"
}

#include <esp_heap_caps.h>
#include <setjmp.h>
#include <stdio.h>

// PERF-H2: Ring-buffer for Doom key events — O(1) dequeue vs O(256) table scan.
// Only 12 physical buttons exist; a 16-slot ring (power-of-2 for fast masking) is plenty.
struct DoomKeyEvent { uint8_t key; uint8_t pressed; };
static DoomKeyEvent s_keyRing[16];
static int          s_keyRingHead = 0;
static int          s_keyRingTail = 0;
// Per-key edge-detect: 0 = idle/released, 1 = pressed/held.
static uint8_t      s_keyState[256];

static size_t  s_targetZoneBytes = DoomEmu::DOOM_ZONE_FLOOR_BYTES;
static jmp_buf s_doomBailoutJmp;
static bool    s_doomJmpActive = false;
static bool    s_doomInitialized = false;
static char    s_lastError[96] = "";

static void showDoomErrorToast() {
    char message[40];
    const char* reason = s_lastError[0] ? s_lastError : "ENGINE ERROR";
    snprintf(message, sizeof(message), "DOOM: %.31s", reason);
    DisplayEmu::showToast(message, Theme::ACCENT_RED, 3000);
}

extern "C" {
    void Doom_Bailout(const char* msg);

    void* Doom_MallocPSRAM(size_t size) {
        void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (!ptr) {
            LOG_ERROR("Failed to allocate %d bytes in PSRAM for DOOM!", (int)size);
            if (s_doomJmpActive) {
                Doom_Bailout("DOOM PSRAM allocation failed");
            }
        }
        return ptr;
    }

    void Doom_FreePSRAM(void* ptr) {
        if (ptr) {
            heap_caps_free(ptr);
        }
    }

    size_t Doom_GetTargetZoneBytes(void) {
        return s_targetZoneBytes;
    }

    void Doom_Bailout(const char* msg) {
        if (msg && msg[0]) {
            snprintf(s_lastError, sizeof(s_lastError), "%s", msg);
        } else {
            snprintf(s_lastError, sizeof(s_lastError), "DOOM engine error");
        }
        LOG_ERROR("[DOOM] Engine bailout: %s", s_lastError);
        if (s_doomJmpActive) {
            s_doomJmpActive = false;
            longjmp(s_doomBailoutJmp, 1);
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

const char* lastError() {
  return s_lastError;
}

bool begin(const char* wadPath) {
  if (s_doomInitialized) {
    destroy();
  }
  s_doomInitialized = false;
  s_lastError[0] = '\0';
  if (!wadPath || wadPath[0] == '\0') {
    snprintf(s_lastError, sizeof(s_lastError), "EMPTY WAD PATH");
    return false;
  }

  size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  size_t zoneBytes = computeZoneBytes(largestBlock);
  LOG_INFO("[DOOM] PSRAM pre-launch: free=%u B, largest_block=%u B, target_zone=%u B",
           (unsigned)freePsram, (unsigned)largestBlock, (unsigned)zoneBytes);
  if (zoneBytes < DOOM_ZONE_FLOOR_BYTES) {
    snprintf(s_lastError, sizeof(s_lastError), "PSRAM BLOCK TOO SMALL FOR DOOM");
    LOG_ERROR("[DOOM] Largest free PSRAM block (%u B) below floor (%u B required)",
              (unsigned)largestBlock, (unsigned)(DOOM_ZONE_FLOOR_BYTES + DOOM_ZONE_MARGIN_BYTES));
    DisplayEmu::showToast("DOOM: LOW PSRAM", Theme::ACCENT_RED, 2200);
    return false;
  }
  s_targetZoneBytes = zoneBytes;

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
    snprintf(s_lastError, sizeof(s_lastError), "WAD READ ERROR");
    LOG_ERROR("[DOOM] Failed to open or read WAD header: %s", s_wadPath);
    DisplayEmu::showToast("DOOM: WAD READ ERROR", Theme::ACCENT_RED, 2200);
    return false;
  }
  if (memcmp(hdr, "IWAD", 4) != 0 && memcmp(hdr, "PWAD", 4) != 0) {
    snprintf(s_lastError, sizeof(s_lastError), "INVALID WAD HEADER");
    LOG_ERROR("[DOOM] Invalid WAD signature in %s", s_wadPath);
    DisplayEmu::showToast("DOOM: INVALID WAD", Theme::ACCENT_RED, 2200);
    return false;
  }
  int32_t numLumps = 0;
  memcpy(&numLumps, hdr + 4, sizeof(int32_t));
  if (numLumps <= 0) {
    snprintf(s_lastError, sizeof(s_lastError), "EMPTY WAD LUMP TABLE");
    LOG_ERROR("[DOOM] Empty lump table in %s", s_wadPath);
    DisplayEmu::showToast("DOOM: EMPTY WAD", Theme::ACCENT_RED, 2200);
    return false;
  }

  s_argv[0] = (char*)"doom";
  s_argv[1] = (char*)"-iwad";
  s_argv[2] = s_wadPath;
  s_argv[3] = NULL;

  s_doomJmpActive = true;
  if (setjmp(s_doomBailoutJmp) != 0) {
    s_doomJmpActive = false;
    doomgeneric_Destroy();
    showDoomErrorToast();
    return false;
  }

  doomgeneric_Create(3, s_argv);
  s_doomJmpActive = false;
  s_doomInitialized = true;

  // Weapon cycling has no default key binding in this port. Bind it after
  // doomgeneric_Create() so a saved config cannot override it.
  key_nextweapon = ']';
  key_prevweapon = '[';
  return true;
}

void runFrame() {
  if (!s_doomInitialized) return;
  // Update button state (handled globally in BmoGameboy.ino loop)

  // Map console buttons to Doom keys
  // A = Fire (CTRL)
  // B = Use/Open (SPACE)
  // SELECT = Map (TAB)
  // START = Enter (ENTER)
  // DPAD = Arrows
  // L / R = Strafe left / right
  // X / Y = Next / previous weapon
  
  updateDoomKey(KEY_RCTRL, Buttons::get(Buttons::A).pressed);
  updateDoomKey(' ',       Buttons::get(Buttons::B).pressed);
  updateDoomKey(KEY_TAB,   Buttons::get(Buttons::SELECT).pressed);
  updateDoomKey(KEY_ENTER, Buttons::get(Buttons::START).pressed);
  updateDoomKey(KEY_UPARROW, Buttons::get(Buttons::UP).pressed);
  updateDoomKey(KEY_DOWNARROW, Buttons::get(Buttons::DOWN).pressed);
  updateDoomKey(KEY_LEFTARROW, Buttons::get(Buttons::LEFT).pressed);
  updateDoomKey(KEY_RIGHTARROW, Buttons::get(Buttons::RIGHT).pressed);
  updateDoomKey(KEY_STRAFE_L, Buttons::get(Buttons::L).pressed);
  updateDoomKey(KEY_STRAFE_R, Buttons::get(Buttons::R).pressed);
  updateDoomKey(']', Buttons::get(Buttons::X).pressed);
  updateDoomKey('[', Buttons::get(Buttons::Y).pressed);

  // Profile DOOM tick latency (this encompasses all internal SD fread()s).
  // DOOM frames target ~28ms (35Hz). If the tick takes >40ms, SD card reads
  // are likely stalling the SPI bus.
  unsigned long startTick = millis();
  s_doomJmpActive = true;
  if (setjmp(s_doomBailoutJmp) == 0) {
    doomgeneric_Tick();
    s_doomJmpActive = false;
  } else {
    s_doomJmpActive = false;
    s_doomInitialized = false;
    doomgeneric_Destroy();
    showDoomErrorToast();
    return;
  }
  unsigned long elapsedTick = millis() - startTick;
  
  if (elapsedTick > 40) {
    LOG_WARN("DOOM latency spike detected: %lu ms", elapsedTick);
  }
}

void destroy() {
  DisplayEmu::waitForDisplay();
  if (s_doomInitialized) {
    s_doomInitialized = false;
    doomgeneric_Destroy();
  }
  memset(s_keyState, 0, sizeof(s_keyState));
  s_keyRingHead = s_keyRingTail = 0;
}

} // namespace DoomEmu

#else // BMO_ENABLE_DOOM == 0

namespace DoomEmu {

static const char* const kDoomDisabledMsg = "DOOM disabled (build with BMO_ENABLE_DOOM=1)";

const char* lastError() {
  return kDoomDisabledMsg;
}

bool begin(const char* wadPath) {
  (void)wadPath;
  LOG_WARN_STR("[DOOM] Engine disabled in MIT default build (set BMO_ENABLE_DOOM=1 for GPL build)");
  DisplayEmu::showToast("DOOM DISABLED (MIT BUILD)", Theme::ACCENT_RED, 2200);
  return false;
}

void runFrame() {}

void destroy() {}

} // namespace DoomEmu

#endif // BMO_ENABLE_DOOM
