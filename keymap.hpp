#pragma once

#include "periph_conf.h"        // for MATRIX_ROWS and MATRIX_COLS

#include <variant>



class keymap_t {
public:
    typedef void (*map_t)(bool);

    keymap_t(uint8_t key): key_map(key) {}
    keymap_t(map_t map): key_map(map) {}

    void handle(bool pressed) const;

private:
    const std::variant<uint8_t, map_t> key_map;
// Todo: Inheritance instead of variant.
//   - each entry of keymap is a pointer to keymap_t.
//   - keymap_t can be inherited to be customized.
//   - when populating keymap[][], a generator (i.e. convertor) is called.
//   - Can be derived from a modifier key to check if the mod is being pressed?
};

extern keymap_t keymap[MATRIX_ROWS][MATRIX_COLS];


/*
// Default keymap
uint8_t keymap[MATRIX_ROWS][MATRIX_COLS] = {
    KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC, KC_DEL,  \

    KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS, KC_HOME, \

    KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_NO,   KC_ENT,  KC_PGUP, \

    KC_LSFT, KC_NO,   KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_RSFT, KC_UP,   KC_PGDN, \

    KC_LCTL, KC_LGUI, KC_LALT, KC_NO,   KC_NO,   KC_NO,   KC_SPC,  KC_NO,   KC_NO,   KC_NO,   KC_RALT, KC_TRNS, KC_LEFT, KC_DOWN, KC_RGHT  \
};
*/

// KC_NO = no key is assigned to the gpio pin.
// KC_TRNS = FN key.
