#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "literal.hpp"
#include "map.hpp"
#include "mod_morph.hpp"
#include "tap_hold.hpp"
#include "timer.hpp"



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

map_t FN;

tap_hold_t<HOLD_PREFERRED> t_LCTL { ESC, LCTL };

tap_hold_t<HOLD_PREFERRED> t_ENT { ENT, FN };

tap_hold_t<BALANCED> t_SPC { SPC, RSFT };

mod_morph_t m_LSFT { LSFT, SPC, t_SPC };

// Here m_BKSP has RSFT as its modifier. With t_SPC instead, when m_BKSP is pressed
// within the tapping term of t_SPC the tap version of t_SPC (i.e. SPC) would be sent
// immediately after sending DEL.
mod_morph_t<UNDO_MOD> m_BKSP { BKSP, DEL, RSFT };

mod_morph_t m_GRV { GRV, POWER, FN };

tap_hold_t t_1 { _1, F1 };        // sizeof = 60
mod_morph_t m_1 { t_1, F1, FN };  // sizeof = 24
tap_hold_t t_2 { _2, F2 };
mod_morph_t m_2 { t_2, F2, FN };
tap_hold_t t_3 { _3, F3 };
mod_morph_t m_3 { t_3, F3, FN };
tap_hold_t t_4 { _4, F4 };
mod_morph_t m_4 { t_4, F4, FN };
tap_hold_t t_5 { _5, F5 };
mod_morph_t m_5 { t_5, F5, FN };
tap_hold_t t_6 { _6, F6 };
mod_morph_t m_6 { t_6, F6, FN };
tap_hold_t t_7 { _7, F7 };
mod_morph_t m_7 { t_7, F7, FN };
tap_hold_t t_8 { _8, F8 };
mod_morph_t m_8 { t_8, F8, FN };
tap_hold_t t_9 { _9, F9 };
mod_morph_t m_9 { t_9, F9, FN };
tap_hold_t t_0 { _0, F10 };
mod_morph_t m_0 { t_0, F10, FN };

tap_hold_t t_MINUS { MINUS, F11 };
mod_morph_t m_MINUS { t_MINUS, F11, FN };
tap_hold_t t_EQL { EQL, F12 };
mod_morph_t m_EQL { t_EQL, F12, FN };

// More ideas:
//  - Left + Left == Ctrl + Left
//  - Left + Left + Left == Ctrl + Left + Left



class test_t: public map_t, public timer_t {
public:
    constexpr test_t(): timer_t(500 *US_PER_MS) {}

private:
    void on_press(pmap_t* slot) {
        start_timer(slot);
        start_defer_presses();
    }

    void on_release(pmap_t*) {
        stop_timer();
        stop_defer_presses();

        if ( FN.is_pressing() && t_LCTL.is_pressing() )
            system_reset();

        // DEBUG("test: map_t=%d literal_t=%d timer_t=%d tap_hold_t=%d mod_morph_t=%d\n",
        //     sizeof(map_t), sizeof(literal_t), sizeof(timer_t),
        //     sizeof(tap_hold_t<HOLD_PREFERRED>), sizeof(mod_morph_t<UNDO_MOD>));

        // Test key sequence
        // send_press(KC_SCOLON);
        // send_press(KC_F);
        // send_press(KC_A);
        // send_release(KC_SCOLON);
        // send_release(KC_F);
        // send_release(KC_A);
    }

    void on_timeout(pmap_t*) {
        if ( FN.is_pressing() ) {
            if ( t_LCTL.is_pressing() )
                // assert( false );
                WDT->CLEAR.reg = 0;  // anything other than 0xA5
            else
                perform_usbhub_switchover();
        }
    }
} test;



pmap_t maps[MATRIX_ROWS][MATRIX_COLS] = {
    m_GRV, m_1, m_2, m_3, m_4, m_5, m_6, m_7, m_8, m_9, m_0, m_MINUS, m_EQL, m_BKSP, HOME,

    TAB, Q, W, E, R, T, Y, U, I, O, P, LBRKT, RBRKT, BSLASH, END,

    t_LCTL, A, S, D, F, G, H, J, K, L, COLON, QUOTE, ___, t_ENT, PGUP,

    m_LSFT, ___, Z, X, C, V, B, N, M, COMMA, DOT, SLASH, RSFT, UP, PGDN,

    FN, LGUI, LALT, ___, ___, ___, t_SPC, ___, ___, ___,
        RCTL, test /*RALT*/, LEFT, DOWN, RIGHT,
};

}  // namespace key
