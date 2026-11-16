// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

namespace BmoAppMenus {

/** Handle the non-blocking boot splash screen state. */
void handleBootSplash();

/** Handle console carousel input, idle timeout, and menu rendering. */
void handleConsoleMenu();

/** Handle the console hardware history / museum modal overlay. */
void handleConsoleMuseum();

/** Handle game selection list input, box art previews, and ROM launches. */
void handleGameMenu();

/** Handle the interactive BMO idle mascot face, Konami easter egg, and gaze. */
void handleIdleMascot();

} // namespace BmoAppMenus
