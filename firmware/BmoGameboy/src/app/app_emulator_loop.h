// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

namespace BmoAppEmulatorLoop {

/** Step active emulator core frame, hotkeys, and frame pacing. */
void handleEmulator();

/** Handle pause menu navigation, save states, battery RAM, and quit flow. */
void handlePauseMenu();

} // namespace BmoAppEmulatorLoop
