#pragma once
#include <stdint.h>

// User will provide the actual bitmap data in these arrays.
// Dimensions are 320x240 = 76800 pixels per frame.
#define BMO_FACE_WIDTH 320
#define BMO_FACE_HEIGHT 240
#define BMO_FACE_PIXELS (BMO_FACE_WIDTH * BMO_FACE_HEIGHT)

extern const uint16_t BMO_FRAME_IDLE[BMO_FACE_PIXELS];
extern const uint16_t BMO_FRAME_BLINK[BMO_FACE_PIXELS];
extern const uint16_t BMO_FRAME_HAPPY[BMO_FACE_PIXELS];
extern const uint16_t BMO_FRAME_SLEEPY[BMO_FACE_PIXELS];
extern const uint16_t BMO_FRAME_LOW_BATTERY[BMO_FACE_PIXELS];
extern const uint16_t BMO_FRAME_CHARGING[BMO_FACE_PIXELS];
extern const uint16_t BMO_FRAME_ERROR[BMO_FACE_PIXELS];
extern const uint16_t BMO_FRAME_SHUTDOWN[BMO_FACE_PIXELS];
