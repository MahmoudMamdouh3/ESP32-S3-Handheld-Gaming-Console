#pragma once

// Peanut-GB feature toggles. These MUST be identical in every translation
// unit that includes peanut_gb.h (emulator.cpp, display_emu.cpp), because
// they affect the layout of struct gb_s. A mismatch is silent undefined
// behavior across translation units.
#define ENABLE_SOUND 0
#define ENABLE_LCD   1
