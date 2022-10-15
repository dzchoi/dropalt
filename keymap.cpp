#define ENABLE_DEBUG 1
#include "debug.h"

#include <atomic>
#include "hid_keycodes.hpp"
#include "keymap.hpp"
#include "usb_thread.hpp"



void keymap_t::handle(bool pressed) const
{
    if ( key_map.index() == 0 ) {
        const uint8_t key = std::get<uint8_t>(key_map);
        usb_thread::obj().hid_keyboard.key_event(key, pressed);
        // DEBUG("Matrix: reported key (0x%x) %s\n", key, pressed ? "press" : "release");
    } else {
        const map_t map = std::get<map_t>(key_map);
        map(pressed);
        // DEBUG("Matrix: processed a key %s\n", pressed ? "press" : "release");
    }
}



void Reset(bool pressed) {
    if ( pressed )
        system_reset();
};

std::atomic<bool> fn_held = false;

void Fn(bool pressed) {
    fn_held = pressed;
}

void Flash(bool pressed) {
    static uint32_t since;
    if ( fn_held ) {
        if ( pressed )
            since = xtimer_now_usec();
        else if ( xtimer_now_usec() - since > 4 *US_PER_SEC )
            WDT->CLEAR.reg = 0;  // anything other than 0xA5
    } else {
        usb_thread::obj().hid_keyboard.key_event(KC_B, pressed);
    }
}



keymap_t keymap[MATRIX_ROWS][MATRIX_COLS] = {
    KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC, KC_DEL,  \

    KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS, KC_HOME, \

    KC_LCTL, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_NO,   KC_ENT,  KC_PGUP, \

    KC_LSFT, KC_NO,   KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_RSFT, KC_UP,   KC_PGDN, \

    Fn,      KC_LGUI, KC_LALT, KC_NO,   KC_NO,   KC_NO,   KC_SPC,  KC_NO,   KC_NO,   KC_NO,   KC_RALT, KC_TRNS, KC_LEFT, KC_DOWN, KC_RGHT  \
};
