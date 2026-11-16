// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

struct RomFile;

namespace BmoAppLauncher {

/** Validate platform flags, load ROM payload, verify identity, and start core. */
void launchSelectedRom(const RomFile* selectedRom);

} // namespace BmoAppLauncher
