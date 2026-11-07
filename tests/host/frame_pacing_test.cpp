#include "frame_pacing.h"

static_assert(FramePacing::targetFrameUs(FramePacing::WALNUT_GBC) == 16742U,
              "GBC cadence must match the Game Boy core timing constant");
static_assert(FramePacing::targetFrameUs(FramePacing::PEANUT_GB) == 16742U,
              "GB cadence must match the Game Boy core timing constant");
static_assert(FramePacing::targetFrameUs(FramePacing::DOOM) == 28571U,
              "DOOM cadence must match the vendored 35 Hz TICRATE");
static_assert(FramePacing::targetFrameUs(FramePacing::PICO8) == 33333U,
              "PICO-8 presentation cadence must be 30 Hz");
static_assert(FramePacing::targetFrameUs(14) == 16667U,
              "Unspecified emulator indexes use the default 60 Hz target");

int main() { return 0; }
