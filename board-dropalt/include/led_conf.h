#pragma once

#include <stdint.h>             // for uint8_t



#define CS_LINE_COUNT           16  // CS 1-16: current source outputs
#define SW_LINE_COUNT           12  // SW 1-12: switch current inputs

enum {
    A_1, A_2, A_3, A_4, A_5, A_6, A_7, A_8, A_9, A_10, A_11, A_12, A_13, A_14, A_15, A_16,
    B_1, B_2, B_3, B_4, B_5, B_6, B_7, B_8, B_9, B_10, B_11, B_12, B_13, B_14, B_15, B_16,
    C_1, C_2, C_3, C_4, C_5, C_6, C_7, C_8, C_9, C_10, C_11, C_12, C_13, C_14, C_15, C_16,
    D_1, D_2, D_3, D_4, D_5, D_6, D_7, D_8, D_9, D_10, D_11, D_12, D_13, D_14, D_15, D_16,
    E_1, E_2, E_3, E_4, E_5, E_6, E_7, E_8, E_9, E_10, E_11, E_12, E_13, E_14, E_15, E_16,
    F_1, F_2, F_3, F_4, F_5, F_6, F_7, F_8, F_9, F_10, F_11, F_12, F_13, F_14, F_15, F_16,
    G_1, G_2, G_3, G_4, G_5, G_6, G_7, G_8, G_9, G_10, G_11, G_12, G_13, G_14, G_15, G_16,
    H_1, H_2, H_3, H_4, H_5, H_6, H_7, H_8, H_9, H_10, H_11, H_12, H_13, H_14, H_15, H_16,
    I_1, I_2, I_3, I_4, I_5, I_6, I_7, I_8, I_9, I_10, I_11, I_12, I_13, I_14, I_15, I_16,
    J_1, J_2, J_3, J_4, J_5, J_6, J_7, J_8, J_9, J_10, J_11, J_12, J_13, J_14, J_15, J_16,
    K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9, K_10, K_11, K_12, K_13, K_14, K_15, K_16,
    L_1, L_2, L_3, L_4, L_5, L_6, L_7, L_8, L_9, L_10, L_11, L_12, L_13, L_14, L_15, L_16,
    SW_CS_TO_REGISTER_END
};

// LED mapping table:
// Each entry maps a logical led_id to:
//  - IS31FL3733 driver index (0 or 1)
//  - PWM register number for the Green channel
// Note:
//  - led_id + 1 corresponds to the LED number silkscreened on the PCB
//  - RGB register offsets follow the IS31FL3733 standard:
//    reg_r = reg_g + CS_LINE_COUNT and reg_b = reg_r + CS_LINE_COUNT
//    E.g. if reg_g = A_2, then reg_r = B_2 and reg_b = C_2.
typedef struct { uint8_t driver; uint8_t reg_g; } is31_led_t;
static_assert( sizeof(is31_led_t) == 2 );
static const is31_led_t IS31_LEDS[] = {
    // /* led_id */ { driver,  reg_g },  // associated key name
    /*  0 */  { 1, A_2 },   // Esc
    /*  1 */  { 1, D_3 },   // 1
    /*  2 */  { 1, D_4 },   // 2
    /*  3 */  { 1, D_5 },   // 3
    /*  4 */  { 1, D_6 },   // 4
    /*  5 */  { 1, D_7 },   // 5
    /*  6 */  { 1, D_8 },   // 6
    /*  7 */  { 1, A_13 },  // 7
    /*  8 */  { 0, D_1 },   // 8
    /*  9 */  { 0, D_2 },   // 9
    /* 10 */  { 0, D_3 },   // 0
    /* 11 */  { 0, D_4 },   // -
    /* 12 */  { 0, D_5 },   // =
    /* 13 */  { 0, D_6 },   // Bksp
    /* 14 */  { 0, A_7 },   // Del
    /* 15 */  { 1, D_2 },   // Tab
    /* 16 */  { 1, G_3 },   // Q
    /* 17 */  { 1, G_4 },   // W
    /* 18 */  { 1, G_5 },   // E
    /* 19 */  { 1, G_6 },   // R
    /* 20 */  { 1, G_7 },   // T
    /* 21 */  { 1, G_8 },   // Y
    /* 22 */  { 0, G_1 },   // U
    /* 23 */  { 0, G_2 },   // I
    /* 24 */  { 0, G_3 },   // O
    /* 25 */  { 0, G_4 },   // P
    /* 26 */  { 0, G_5 },   // [
    /* 27 */  { 0, J_11 },  // ]
    /* 28 */  { 0, G_6 },   // '\'
    /* 29 */  { 0, D_7 },   // Home
    /* 30 */  { 1, G_2 },   // Capslock
    /* 31 */  { 1, J_3 },   // A
    /* 32 */  { 1, J_4 },   // S
    /* 33 */  { 1, J_5 },   // D
    /* 34 */  { 1, J_6 },   // F
    /* 35 */  { 1, J_7 },   // G
    /* 36 */  { 1, J_8 },   // H
    /* 37 */  { 0, J_1 },   // J
    /* 38 */  { 0, J_2 },   // k
    /* 39 */  { 0, J_3 },   // L
    /* 40 */  { 0, J_4 },   // ;
    /* 41 */  { 0, J_5 },   // '
    /* 42 */  { 0, J_6 },   // Enter
    /* 43 */  { 0, G_7 },   // PgUp
    /* 44 */  { 1, J_2 },   // LShft
    /* 45 */  { 1, D_9 },   // Z
    /* 46 */  { 1, A_9 },   // X
    /* 47 */  { 1, J_9 },   // C
    /* 48 */  { 1, G_9 },   // V
    /* 49 */  { 1, J_12 },  // B
    /* 50 */  { 1, J_13 },  // N
    /* 51 */  { 0, J_9 },   // M
    /* 52 */  { 0, J_10 },  // ,
    /* 53 */  { 0, G_10 },  // .
    /* 54 */  { 0, G_11 },  // /
    /* 55 */  { 0, A_11 },  // RShft
    /* 56 */  { 0, D_11 },  // Up
    /* 57 */  { 0, J_7 },   // PgDn
    /* 58 */  { 1, G_10 },  // LCtrl
    /* 59 */  { 1, D_10 },  // Win
    /* 60 */  { 1, A_10 },  // LAlt
    /* 61 */  { 1, G_12 },  // Space
    /* 62 */  { 0, D_10 },  // RAlt
    /* 63 */  { 0, A_10 },  // Fn
    /* 64 */  { 0, A_12 },  // Left
    /* 65 */  { 0, G_12 },  // Down
    /* 66 */  { 0, D_12 },  // Right

    // Underglow LEDs
    /* 67 */  { 1, J_11 },  // bottom-left corner
    /* 68 */  { 1, G_11 },
    /* 69 */  { 1, D_11 },
    /* 70 */  { 1, A_11 },
    /* 71 */  { 1, A_12 },
    /* 72 */  { 1, D_12 },
    /* 73 */  { 1, D_13 },
    /* 74 */  { 1, G_13 },
    /* 75 */  { 0, G_9 },
    /* 76 */  { 0, D_9 },
    /* 77 */  { 0, A_9 },
    /* 78 */  { 0, A_13 },
    /* 79 */  { 0, G_13 },
    /* 80 */  { 0, D_13 },
    /* 81 */  { 0, J_13 },  // bottom-right corner
    /* 82 */  { 0, J_12 },
    /* 83 */  { 0, J_8 },
    /* 84 */  { 0, G_8 },
    /* 85 */  { 0, D_8 },
    /* 86 */  { 0, A_8 },   // top-right corner
    /* 87 */  { 0, A_6 },
    /* 88 */  { 0, A_5 },
    /* 89 */  { 0, A_4 },
    /* 90 */  { 0, A_3 },
    /* 91 */  { 0, A_2 },
    /* 92 */  { 0, A_1 },
    /* 93 */  { 0, A_14 },
    /* 94 */  { 1, A_8 },
    /* 95 */  { 1, A_7 },
    /* 96 */  { 1, A_6 },
    /* 97 */  { 1, A_5 },
    /* 98 */  { 1, A_4 },
    /* 99 */  { 1, A_3 },
    /* 100 */ { 1, A_1 },   // top-left corner
    /* 101 */ { 1, D_1 },
    /* 102 */ { 1, G_1 },
    /* 103 */ { 1, J_1 },
    /* 104 */ { 1, J_10 },
};

static const unsigned ALL_LED_COUNT = sizeof(IS31_LEDS) / sizeof(IS31_LEDS[0]);
static const unsigned KEY_LED_COUNT = 67;
static const unsigned UNDERGLOW_COUNT = ALL_LED_COUNT - KEY_LED_COUNT;
