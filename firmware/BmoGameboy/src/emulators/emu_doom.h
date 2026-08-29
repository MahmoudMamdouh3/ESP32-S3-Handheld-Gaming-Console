#pragma once
#include <stdint.h>
#include <stddef.h>

namespace DoomEmu {
  // Begin the Doom engine.
  // wadPath must be the absolute path to the WAD on the VFS (e.g. "/sd/DOOM1.WAD")
  bool begin(const char* wadPath);
  
  // Run one frame/tick of Doom
  void runFrame();
  
  // Shut down Doom
  void destroy();
}
