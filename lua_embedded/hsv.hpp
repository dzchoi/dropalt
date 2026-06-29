#pragma once

#include "fast_hsv2rgb.h"       // for HSV_HUE_STEPS



// Perceptual lightness curve approximating the CIE 1931 luminance formula. The true
// curve is roughly gamma ≈ 2.3; plain γ = 2 (out = round(v² / 255)) is a close enough
// approximation here.
inline constexpr uint8_t cie1931(uint8_t v)
{
    return (v * v + 127) / 255;
}

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
