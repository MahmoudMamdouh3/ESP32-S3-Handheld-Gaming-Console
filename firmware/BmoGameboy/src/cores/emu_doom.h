#pragma once
#include <stdint.h>
#include <stddef.h>
#include "psram_zone_budget.h"

namespace DoomEmu {
  // Last failure reason string
  const char* lastError();

  // Begin the Doom engine.
  // wadPath must be the absolute path to the WAD on the VFS (e.g. "/sd/DOOM1.WAD")
  bool init(const char* wadPath);

  // Run one frame/tick of Doom
  void update();

  // Shut down Doom
  void destroy();
}
