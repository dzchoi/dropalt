#define ENABLE_DEBUG 1
#include "debug.h"

// Todo: namespace?
#include <atomic>
#include "hid_keycodes.hpp"
#include "keymap.hpp"
#include "xtimer_wrapper.hpp"



class tap_hold_t: public keymap_t {
public:
    void operator()(bool pressed) {
        if ( pressed ) {
            m_holding = false;
            timer.start();
        }
        else if ( m_holding ) {
            usb_thread::obj().hid_keyboard.report_release(m_code_hold);
        }
        else {
            timer.stop();
            usb_thread::obj().hid_keyboard.report_tap(m_code_tap);
        }
    }

    template <uint8_t CODE_TAP, uint8_t CODE_HOLD>
    constexpr tap_hold_t(Key<CODE_TAP>, Key<CODE_HOLD>)
    : m_code_tap(CODE_TAP), m_code_hold(CODE_HOLD) {}

private:
    const uint8_t m_code_tap;
    const uint8_t m_code_hold;
    std::atomic<bool> m_holding = false;

    static void _tmo_tapping_not_detected(void* arg) {
        tap_hold_t* const that = static_cast<tap_hold_t*>(arg);
        usb_thread::obj().hid_keyboard.report_press(that->m_code_hold);
        that->m_holding = true;
    }
    xtimer_onetime_callback_t timer { TAPPING_TERM_US, _tmo_tapping_not_detected, this };
};

tap_hold_t EXT_LCTL { KC_ESC, KC_LCTL };
tap_hold_t EXT_SPC { KC_SPC, KC_RSFT };



constexpr auto ____ = KC_NO;
// constexpr auto XXXXXXX = KC_NO;

// Todo: Allocate them in PROGMEM(?).
rkeymap_t keymap[MATRIX_ROWS][MATRIX_COLS] = {
    KC_GRAVE, KC_1, KC_2, KC_3, KC_4, KC_5,
    KC_6, KC_7, KC_8, KC_9, KC_0,
    KC_MINS, KC_EQL, KC_BSPC, KC_DEL,

    KC_TAB, KC_Q, KC_W, KC_E, KC_R, KC_T,
    KC_Y, KC_U, KC_I, KC_O, KC_P,
    KC_LBRC, KC_RBRC, KC_BSLS, KC_HOME,

    EXT_LCTL, KC_A, KC_S, KC_D, KC_F, KC_G,
    KC_H, KC_J, KC_K, KC_L,
    KC_SCLN, KC_QUOT, ____, KC_ENT, KC_PGUP,

    KC_LSFT, ____, KC_Z, KC_X, KC_C, KC_V, KC_B,
    KC_N, KC_M, KC_COMM, KC_DOT,
    KC_SLSH, KC_RSFT, KC_UP, KC_PGDN,

    KC_TRNS, KC_LGUI, KC_LALT,
    ____, ____, ____, EXT_SPC, ____, ____, ____,
    KC_RALT, KC_TRNS, KC_LEFT, KC_DOWN, KC_RGHT
};

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
