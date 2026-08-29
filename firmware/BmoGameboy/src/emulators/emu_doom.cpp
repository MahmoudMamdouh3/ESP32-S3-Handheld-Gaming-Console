#include "emu_doom.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"

extern "C" {
#include "../vendor/doom/src/doomgeneric.h"
#include "../vendor/doom/src/doomkeys.h"
}

#include <Arduino.h>
#include <esp_heap_caps.h>

// Globals to track key state for Doom
static int keys[256] = {0};

extern "C" {
    void* Doom_MallocPSRAM(size_t size) {
        void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (!ptr) {
            Serial.printf("Failed to allocate %d bytes in PSRAM for DOOM!\n", (int)size);
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
  delay(ms);
}

uint32_t DG_GetTicksMs() {
  return millis();
}

int DG_GetKey(int* pressed, unsigned char* key) {
  // Find a key that was just pressed or released
  for (int i = 0; i < 256; i++) {
    if (keys[i] > 0) {
      if (keys[i] == 1) { // Pressed
        *pressed = 1;
        *key = i;
        keys[i] = 2; // Mark as held
        return 1;
      } else if (keys[i] == 3) { // Released
        *pressed = 0;
        *key = i;
        keys[i] = 0; // Clear state
        return 1;
      }
    }
  }
  return 0; // No key events
}

void DG_SetWindowTitle(const char * title) {
  // Not used in embedded
}

} // extern "C"

// Helper to push key events
static void updateDoomKey(int doomKey, bool pressed) {
  if (pressed) {
    if (keys[doomKey] == 0) keys[doomKey] = 1; // Just pressed
  } else {
    if (keys[doomKey] == 2) keys[doomKey] = 3; // Just released
  }
}

namespace DoomEmu {

bool begin(const char* wadPath) {
  // Initialize keyboard state
  memset(keys, 0, sizeof(keys));

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
  // Update button state
  Buttons::update();
  
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
    Serial.printf("DOOM latency spike detected: %lu ms\n", elapsedTick);
  }
}

void destroy() {
  // Doom engine doesn't have a clean destroy, we just let it be.
}

}
