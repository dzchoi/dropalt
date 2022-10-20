#include "basic.hpp"
#include "taphold.hpp"



namespace key {

/*
// Default keymap for reference
uint8_t keymap[MATRIX_ROWS][MATRIX_COLS] = {
    KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC, KC_DEL,  \

    KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS, KC_HOME, \

    KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_NO,   KC_ENT,  KC_PGUP, \

    KC_LSFT, KC_NO,   KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_RSFT, KC_UP,   KC_PGDN, \

    KC_LCTL, KC_LGUI, KC_LALT, KC_NO,   KC_NO,   KC_NO,   KC_SPC,  KC_NO,   KC_NO,   KC_NO,   KC_RALT, KC_TRNS, KC_LEFT, KC_DOWN, KC_RGHT  \
};
*/

tap_hold_t ext_LCTL { ESC, LCTL };
tap_hold_t ext_SPC { SPC, RSFT };
// Under context:
//  = SPC + BKSP == DEL
//  - SPC + LSFT == LSFT + SPC



// Todo: Allocate them in PROGMEM(?).
pmap_t maps[MATRIX_ROWS][MATRIX_COLS] = {
    GRV, _1, _2, _3, _4, _5, _6, _7, _8, _9, _0, MINUS, EQL, BKSP, DEL,

    TAB, Q, W, E, R, T, Y, U, I, O, P, LBRKT, RBRKT, BSLASH, HOME,

    ext_LCTL, A, S, D, F, G, H, J, K, L, COLON, QUOTE, ____, ENT, PGUP,

    LSFT, ____, Z, X, C, V, B, N, M, COMMA, DOT, SLASH, RSFT, UP, PGDN,

    NO, LGUI, LALT, ____, ____, ____, ext_SPC, ____, ____, ____,
        RCTL, RALT, LEFT, DOWN, RIGHT,
};

}  // namespace key
