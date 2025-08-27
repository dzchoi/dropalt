#pragma once

#include "fast_hsv2rgb.h"       // for HSV_HUE_STEPS



// Perceptual lightness curve based on the CIE 1931 luminance formula.
// Generated via Python script from http://jared.geek.nz/2013/feb/linear-led-pwm
// Maps linear 0–255 LED input to gamma-corrected brightness for human eye response.
inline constexpr uint8_t CIE1931_CURVE[256] = {
    0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,
    2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,
    4,   4,   5,   5,   5,   5,   5,   6,   6,   6,   6,   6,   7,   7,   7,   7,
    7,   8,   8,   8,   8,   9,   9,   9,   9,  10,  10,  10,  11,  11,  11,  12,
   12,  12,  13,  13,  13,  14,  14,  14,  15,  15,  15,  16,  16,  17,  17,  17,
   18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  23,  24,  24,  25,  25,
   26,  26,  27,  27,  28,  29,  29,  30,  30,  31,  32,  32,  33,  34,  34,  35,
   36,  36,  37,  38,  38,  39,  40,  41,  41,  42,  43,  44,  45,  45,  46,  47,
   48,  49,  50,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
   63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  76,  77,  78,  79,
   80,  81,  83,  84,  85,  86,  88,  89,  90,  91,  93,  94,  95,  97,  98, 100,
  101, 102, 104, 105, 107, 108, 109, 111, 112, 114, 115, 117, 119, 120, 122, 123,
  125, 126, 128, 130, 131, 133, 135, 136, 138, 140, 142, 143, 145, 147, 149, 150,
  152, 154, 156, 158, 160, 162, 163, 165, 167, 169, 171, 173, 175, 177, 179, 181,
  183, 186, 188, 190, 192, 194, 196, 198, 201, 203, 205, 207, 209, 212, 214, 216,
  219, 221, 223, 226, 228, 231, 233, 235, 238, 240, 243, 245, 248, 250, 253, 255
};


// Note: Hue range is [0, 0x600) rather than [0, 360°).
//  - High byte (0-5) selects sextant: R->Y->G->C->B->P.
//  - Low byte (0-255) represents position within the sextant.
//  - HSV_HUE_STEPS = 6 sextants × 256 steps each.

// https://stackoverflow.com/questions/21737613/image-of-hsv-color-wheel-for-opencv
constexpr uint16_t ORANGE = 30 * HSV_HUE_STEPS / 360;
constexpr uint16_t SPRING_GREEN = 90 * HSV_HUE_STEPS / 360;
constexpr uint16_t MILD_GREEN = 120 * HSV_HUE_STEPS / 360;

// White: h=0, s=0, v=255
// Black: h=0, s=0, v=0
